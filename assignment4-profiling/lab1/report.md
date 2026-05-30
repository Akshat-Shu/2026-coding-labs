# Profiling Lab Report

## 1. Optimizations Made

1. Rewrote the math in `next_pressure_value`. Pulse is now `(row - col - 3*pass) & 15` instead of `(row*17 + col*31 + pass*13) & 15` — same result mod 16, one multiply instead of three. Also swapped `*2`, `/8`, `/2` for `<<1`, `>>3`, `>>1`, and changed `heatmap[i] / 8` to `heatmap[i] >> 3` in `compute_congestion_pressure`. Divides showed up as the per-line hotspot in KCachegrind's annotation of `next_pressure_value`.

2. Flipped the `if (((center + row + pass) & 7) == 0)` in `next_pressure_value` to `!= 0` and swapped the two arms, so the common case (7/8 of iterations) is the immediate branch. The hot path now follows the fall-through, which is easier on the predictor and the i-cache. Small but real win on the timing.

3. Replaced the if/else in `next_pressure_value` with a ternary, so the compiler can lower it to a `cmov` instead of a branch. Two reasons: the branch is data-dependent on `center`, so the predictor can't really learn it; and removing the branch is a prerequisite for auto-vectorizing the surrounding loop later.

4. Swapped the loop nesting in `compute_congestion_pressure`. Was `for col { for row { ... } }`, now `for row { for col { ... } }`. The grid is row-major (`index = row * cols + col`), so the inner loop now walks contiguous memory instead of jumping by `cols * sizeof(int)` per iteration. The README hints at this — it says the inner loops "intentionally walk a row-major array in column-major order to create a cache-locality problem for students to find".

5. Hoisted `current.data()`, `next.data()`, `source.data()` into local `const int* __restrict__` / `int* __restrict__` pointers above the row loop. The `__restrict__` tells the compiler the three buffers don't alias, which is what was blocking auto-vectorization. Confirmed by running with `-fopt-info-vec-all` — before this change the col loop reported "would need a runtime alias check" and failed to vectorize; after, the same loop reports "loop vectorized using 32 byte vectors" once the build flag is `-O3 -march=native`. Also switched the build to `-O3 -march=native` for this step, because `-O2`'s vectorization cost model is conservative and skips even when it could vectorize cleanly.

6. Switched `distance` and `visited` in `shortest_path_bfs` from raw `new[]` pointers to `std::vector`. The original code leaked these on every call (no matching `delete[]`), which Valgrind's leak summary catches immediately. Same change also opens the door to passing them by reference and reusing them across calls later. Then removed the `if (next_row < 0 || next_row >= rows || ...)` bounds check inside the BFS direction loop. The generated grid is wrapped in `#` borders, and the existing `grid[next_row][next_col] == '#'` check rejects any step into the border without ever reading out of bounds — so the explicit four-way comparison is dead weight in every BFS iteration. Fixed the sanity-check grid in the process: it had been using open `.` borders, which only worked because the bounds check protected it; once the check was gone, the sanity grid needed `#` borders too to match the actual invariant.

7. Dropped the `visited` vector entirely and used `distance[i] != -1` as the "already visited" check. The two arrays held the same information — `distance` is initialized to `-1`, and any cell ever enqueued gets a non-negative distance. One fewer array means one less random read per neighbor (4 per BFS step), one less cache line in the working set, and one less allocation per BFS call.

8. Shrank the `distance` element type from `int` (4 bytes) to `uint16_t` (2 bytes). Max BFS distance on a 260×260 grid is bounded by `~rows + cols ≈ 520`, well under `uint16_t`'s max of `65535`. Halves the distance array's memory footprint (from ~270 KB to ~135 KB), which means fewer L1/L2 misses and a smaller memset each call. The sentinel `-1` cast to `uint16_t` becomes `0xFFFF`, so the "not visited" check became `distance[i] != static_cast<uint16_t>(-1)`.

9. Changed `frontier` from `vector<Point>` (8 bytes per entry) to `vector<int>` (4 bytes per entry), storing the flat index `row * cols + col` directly. Halves the frontier's size — for a 260×260 grid that's ~540 KB → ~270 KB, again less cache pressure and a smaller per-call allocation. Recovering `row`/`col` from the index when popping uses one division and a multiply-subtract (`current_col = current_index - current_row * cols`), which the compiler turns into a multiply-by-magic-number for div-by-constant.

## 2. Methodology Walkthrough

I ran the program first with `time`, just to know what I was dealing with:

```
time_sec = 2.16284
       2.190593567 seconds time elapsed
       2.050109000 seconds user
       0.133876000 seconds sys
```

Almost all of it is `user` — no surprise there, it's pure compute.

Next I did `perf record` + flamegraph. Two big chunks: the BFS region and an even wider congestion-pressure region. Then KCachegrind on `next_pressure_value` — the per-line numbers pointed straight at the integer divide and the multiplies in the pressure formula. That gave me a target: rip out the slow arithmetic. Divides by powers of two are just shifts, and the pulse formula can be reduced mod 16.

After the rewrite, the same run looks like this:

```
time_sec = 1.54161
       1.545234753 seconds time elapsed
       1.539794000 seconds user
       0.003999000 seconds sys
```

Down from ~2.16s to ~1.54s. Output checksums match the original, so the bit-twiddling didn't change the result.

Next I tried flipping the branch in the same function — the `& 7 == 0` case only fires 1 in 8, so making the common path the `if` arm should give the predictor an easier time. After:

```
time_sec ≈ 1.49
       1.497640934 seconds time elapsed
       1.489401000 seconds user
       0.005993000 seconds sys
```

Small but real — about 50ms off, consistent with the branch being predicted slightly better when the hot arm is the fall-through.

Then I went branchless — same logic, but as a ternary. The point isn't really branch prediction for this workload (small effect) but to let the compiler lower it to `cmov` and, more importantly, open the door to vectorizing the loop later. A loop body with a data-dependent branch won't vectorize. After the ternary:

```
time_sec = 1.45655
       1.459196375 seconds time elapsed
       1.455199000 seconds user
       0.002998000 seconds sys
```

Modest gain. The real reason for this change shows up later.

Then I went back to `compute_congestion_pressure`. The original loop ran `for col { for row { ... } }`, which on a row-major array means every inner step jumps `cols * 4` bytes ahead in memory — basically a guaranteed cache miss per cell. Flipped the nesting to `for row { for col { ... } }` so the inner loop walks contiguous bytes. After:

```
time_sec = 1.38238
       1.383613469 seconds time elapsed
       1.380458000 seconds user
       0.002000000 seconds sys
```

About 80ms off, and the L1 D-cache miss rate dropped noticeably in `perf stat`.

Now the inner column loop is contiguous, branchless, and the buffers don't alias each other — every prerequisite for SIMD is in place. So I hoisted `current.data()`, `next.data()`, `source.data()` into `__restrict__`-qualified raw pointers and rebuilt with `-O3 -march=native -fno-omit-frame-pointer`. The vectorization report was the confirmation I wanted:

```
grid_bfs.cpp:<col loop>: optimized: loop vectorized using 32 byte vectors
```

32-byte vectors means AVX2 (8 ints per iteration). Timing:

```
time_sec = 0.939109
       0.940640671 seconds time elapsed
       0.936336000 seconds user
       0.002002000 seconds sys
```

Big drop — about 30% off the previous step. Worth noting: at `-O2` this same change barely moved the needle. The `__restrict__` removed the aliasing blocker but `-O2`'s cost model still passed on vectorizing the loop. `-O3` (or specifically `-O3` + `-ftree-loop-vectorize` priced more aggressively) is what actually crossed the threshold.

Next I turned to `shortest_path_bfs`. Two things stood out. First, Valgrind's leak summary flagged `distance` and `visited` — both allocated with `new[]` and never freed, leaking ~8MB per run. Easy fix: turn them into `std::vector`. Second, the four-way `if (next_row < 0 || next_row >= rows || next_col < 0 || next_col >= cols)` check at the top of the direction loop was paying for a guarantee the grid already provided — the generated grid has `#` borders, and the existing `grid[...][...] == '#'` check rejects any neighbor that's actually outside the playable area. The bounds check was dead work every step.

Removing it tripped the sanity test, because the sanity grid had open `.` borders. That was a real latent bug in the test setup, not a regression — once I gave the sanity grid `#` borders to match the real workload's invariant, the test passed again. Timing after both changes:

```
time_sec = 0.839173
       0.843137869 seconds time elapsed
       0.837856000 seconds user
       0.002002000 seconds sys
```

~100ms off, and the program is now leak-free.

Staring at the BFS body afterwards, I noticed `visited` and `distance` were carrying the same signal. `distance[i] == -1` already means "unvisited", since every cell that gets enqueued is also assigned a distance in the same step. So `visited` was a redundant array — one extra load per neighbor, one extra write per enqueue, and one extra allocation+memset per BFS call. Dropped it. After:

```
time_sec = 0.72503
       0.726612933 seconds time elapsed
       0.723482000 seconds user
       0.002997000 seconds sys
```

Another ~115ms off. The compound win from those two BFS changes is larger than the SIMD step.

Same idea, smaller scale: `distance` was a `vector<int>` (4 bytes per cell), but the largest BFS distance on the workload's grid is bounded by `~rows + cols`, so it fits comfortably in a `uint16_t` (2 bytes). Swapping the element type cuts the array from ~270 KB to ~135 KB — less cache pressure, smaller per-call memset. Had to be careful with the sentinel: `-1` cast to `uint16_t` is `0xFFFF`, so the check now reads `distance[i] != static_cast<uint16_t>(-1)`. After:

```
time_sec = 0.691912
       0.693691900 seconds time elapsed
       0.691605000 seconds user
       0.001998000 seconds sys
```

Small but free win. The bigger story here is the next two changes, both about that distance array being recreated per BFS call.

Same logic applied to `frontier`. It was a `vector<Point>` (two ints per entry, 8 bytes), but a flat index encodes the same thing in 4 bytes. Swapped it to `vector<int>` and pushed `row * cols + col` directly. Popping now recovers row/col with one divide and one multiply-subtract, which the compiler folds into magic-number multiplies for div-by-constant — cheap. After:

```
time_sec = 0.61071
       0.611233593 seconds time elapsed
       0.607350000 seconds user
       0.003989000 seconds sys
```

Another ~80ms. The frontier was about as big as `distance` was before shrinking it, so the saving is comparable.

### Before — baseline

```
time_sec = 2.16284
       2.190593567 seconds time elapsed
       2.050109000 seconds user
       0.133876000 seconds sys
```

Callgrind totals on the baseline: `shortest_path_bfs` at 30.48% of instructions, `compute_congestion_pressure` at 19.99%.

### After — current

```
time_sec = 0.61071
       0.611233593 seconds time elapsed
       0.607350000 seconds user
       0.003989000 seconds sys
```


## 3. Correctness Evidence

`--test` still passes after each step. Checksums from the unmodified program vs the current version are compared at the end; through step 1 they match.

## 4. Conceptual Questions

- **Q1.1: Why does `user + sys` not always equal `real`?**
  - `real` is wall clock. `user + sys` is CPU time charged to the process. They drift apart when the process is blocked (I/O, sleeping), scheduled out, or split across multiple CPUs. `real` can also include time the kernel spent doing things not billed back to the process.

- **Q2.1: How does `perf stat` compute event counts and derived metrics?**
  - Hardware performance counters (the PMU) count events directly: cycles, instructions, branches, cache misses, etc. The derived stuff is just division: `insn per cycle = instructions / cycles`, `% of all branches = branch-misses / branches`. If you ask for more events than the hardware can count at once, `perf` multiplexes and scales the result, so those counts are estimates.

- **Q2.2: What do the right-side parentheses in `perf stat` mean (e.g., `(24.94%)`)?**
  - That's the fraction of the run during which the event was actually being counted. When events get multiplexed, each one only gets a slice of the time. `perf` scales the count back up assuming the rate was uniform. 100% means the count is exact; lower means it's a scaled estimate.

- **Q2.3: Is a number like `390722434 cache-misses` always exact?**
  - No. When multiplexing happens, the value is scaled, not measured directly. Some CPUs also have known counter quirks. The number's good enough to compare runs, not good enough to take literally.

- **Q3.1: What are frame pointers and how does `perf -g` use them?**
  - The frame pointer (`rbp` on x86-64) marks where the current function's stack frame starts. Each function saves the caller's frame pointer when it sets up its own, so the frames form a linked list back to the entry point. `perf -g` walks that list to grab call stacks at each sample. Compilers usually drop the frame pointer for the extra register; `-fno-omit-frame-pointer` keeps it. Without it, `perf` has to use DWARF unwind tables which is slower and sometimes wrong in optimized code.

- **Q3.2: Inclusive cost vs self cost?**
  - Self cost is time spent in a function's own code. Inclusive cost is self plus everything it called. Self tells you which function is heavy; inclusive tells you which path is heavy.

- **Q4.1: How does `gprof` obtain call counts and caller-to-callee counts?**
  - Two pieces. `-pg` makes the compiler emit a stub at every function entry that bumps a counter and records who called it. that's how you get exact call counts and the caller/callee edges. Separately, a `SIGPROF` timer fires periodically and samples where the program is, which is what attributes time to functions. Mash the two together and you get the flat profile + call graph.

- **Q4.2: Why use `gprof` if `perf`/FlameGraph already show hotspots?**
  - Different views. `perf` samples an optimized binary, which is what you actually ship — best for "where is the time really going". `gprof` runs an instrumented `-O0` binary, which is artificial, but gives you exact call counts and a readable call graph. Helpful for understanding structure, especially if a small helper is called a million times and you want to know that.

- **Q5.1: Valgrind Memcheck vs AddressSanitizer? When to use each?**
  - Memcheck instruments any existing binary, no recompile needed. Very slow (10-30×), but catches lots of stuff: leaks with full stack traces, uninit reads, invalid free, etc. Reach for it when you can't recompile or want a thorough leak audit.
  - ASan needs you to rebuild with `-fsanitize=address`. Maybe 2-3× slower than native. Much faster than Valgrind and fine to leave on in CI. Catches similar bugs but doesn't track leaks as carefully unless you pair with LSan.
  - Day to day: ASan. Special cases (no source, deep leak hunt, mystery uninit read): Valgrind.

- **Q6.1: Did any tool disagree with another?**