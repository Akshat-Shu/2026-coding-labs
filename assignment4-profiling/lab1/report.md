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

10. Flattened the grid from `vector<string>` (a vector of separately heap-allocated string objects) to a single `static std::array<char, kRows * kCols>` declared inside `main`. The `static` keyword gives static storage duration without making the variable global, and the buffer lives in BSS — zero-initialized once at program start and contiguous in memory. Switched every function that consumed the grid to take `const char *grid, int rows, int cols` and index by `grid[row * cols + col]`. Eliminated the double pointer-chase per cell read (`vector<string>` → `string::data()` → `data[col]`) that was responsible for a huge fraction of the program's L1 cache misses. Also updated the sanity-check grid to use the same flat layout.

11. Replaced the `drow[4]` / `dcol[4]` arrays with a single `offsets[4] = {-cols, cols, -1, 1}` array. Now that the grid is a flat buffer, neighbor indices are just `current_index + offsets[direction]` — no need to recover `current_row`/`current_col` from the index, no per-neighbor `row * cols + col` multiply. The divide-by-`cols` and the multiply both go away, and the per-direction body shrinks to a single add.

12. Hoisted `distance` and `frontier` out of `shortest_path_bfs` and into `run_all_requests`, passing them in by reference and reusing them across all 1200 BFS calls. Before, each BFS call allocated ~135 KB for `distance` and ~270 KB for `frontier`, then freed them — 1200 times per run, which `perf record` showed as a chunky region of `__memset_avx2_unaligned_erms` and allocator code. After, the buffers are allocated once. `frontier` doesn't need any reset because `frontier_head`/`frontier_tail` handle that. `distance` still needs `std::fill` back to the `-1` sentinel each call, but no allocation cost.

13. Shrank `heatmap` from `vector<int>` to `vector<uint16_t>`. Same idea as the `distance` shrink earlier — max visits in this workload is 957 (visible in the output as `heatmap_max_visits`), so 16 bits is plenty. Halves the heatmap's memory footprint from ~270 KB to ~135 KB. The biggest immediate win is line 224 (`heatmap[next_index] += 1`) in BFS, which Callgrind had flagged as the single largest L1 write-miss line (~32% of all D1 write misses) — that 2-byte RMW per cell is now half as much memory traffic. Also touched `summarize_heatmap`, `compute_congestion_pressure`, and `main` to take/pass `vector<uint16_t>`.

14. Removed the `source` vector in `compute_congestion_pressure`. It was a precomputed copy of `heatmap >> 3` that the inner loop read once per cell. Replaced it with reading `heatmap[index] >> 3` directly inside the loop. Saves the 270 KB allocation, removes the init pass over all 67,600 cells, and the inner loop now reads 2 bytes per cell from `heatmap` (uint16_t after step 13) instead of 4 bytes per cell from `source` (int). Vectorization survives — the col loop still reports "loop vectorized using 32 byte vectors" under `-fopt-info-vec` after the change.

15. Merged the grid blocked-check into the `distance` array via a second sentinel value. Pre-fill `distance_buf` once in `run_all_requests` with `0xFFFE` for blocked cells and `0xFFFF` for open cells. Then the BFS hot loop's two condition checks (`grid[next_index] == '#'` and `distance[next_index] != sentinel`) collapse into one — `distance[next_index] != 0xFFFF` rejects blocked and visited cells alike. The `grid` array is no longer read in the BFS hot path at all. Reset between calls is now a sequential rewrite of the full `distance` array from `grid` (`distance[i] = (grid[i] == '#') ? 0xFFFE : 0xFFFF`). First tried a touched-list reset (only touch cells in `frontier`), which was correct but ~10 ms slower and noisier — the random-indexed writes cost more than a vectorizable sequential pass over all cells, even though the latter does 2.5× as many writes.

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

Going back to callgrind, the biggest single line in the BFS hot path was still `grid[next_row][next_col] == '#'`. `grid` was a `vector<string>` — a vector of separately heap-allocated string objects — which means every cell read walked a pointer to find the row's data buffer, then indexed into it. Two pointer chases, two unrelated cache lines per read. The fix is to flatten the grid to a single contiguous buffer.

Pulled `grid` into a `static std::array<char, kRows * kCols>` declared inside `main`. The `static` keyword gives it static storage duration without making it a global, which is what I wanted — the buffer lives in BSS, gets zero-initialized at program start, and is reachable only from `main` and whatever it passes the pointer into. That meant changing the grid-using functions (`is_open`, `generate_grid`, `next_open_cell`, `generate_requests`, `shortest_path_bfs`, `run_all_requests`, `count_open_cells`, `print_summary`) to take `const char *grid, int rows, int cols` and index by `grid[row * cols + col]`.

Also had to flatten the sanity grid the same way. After:

```
time_sec = 0.659316
       0.660938916 seconds time elapsed
       0.658858000 seconds user
       0.000999000 seconds sys
```

Wall-clock moved less than I expected, but the L1 D-cache miss count dropped by about 30% in `perf stat`, which is the real story — the program is doing the same amount of work but with much better cache behavior. Wall-clock will improve further once the BFS gets reused buffers later.

With the grid flat, `grid[next_index]` just needs `next_index` — there's no reason to keep computing `next_row`/`next_col` at all. Replaced the `drow[4]` / `dcol[4]` pair with a single `offsets[4] = {-cols, cols, -1, 1}` and dropped the `current_index / cols` and `current_index - current_row * cols` recovery code that step 9 had introduced. Each direction is now one add. After:

```
time_sec = 0.646576
       0.650498330 seconds time elapsed
       0.643005000 seconds user
       0.005990000 seconds sys
```

Small recorded improvement on this run, but cleaner code that drops both the divide and the per-neighbor multiply.

Last big chunk left on the callgrind was the per-call allocation in BFS. Every one of the 1200 `shortest_path_bfs` calls was allocating ~135 KB for `distance` and ~270 KB for `frontier`, memset-ing them, and freeing them on return. The subroutine cost of the BFS function was almost 25% of the total runtime, mostly in `__memset_avx2_unaligned_erms` and allocator code.

Hoisted both buffers out to `run_all_requests` and passed them in by reference. `frontier` doesn't need any reset between calls — `frontier_head`/`frontier_tail` handle that. `distance` still needs to go back to `-1` each call, but only a `std::fill`, no allocation. After:

```
time_sec = 0.678469
       0.681875613 seconds time elapsed
       0.677138000 seconds user
       0.003994000 seconds sys
```

End-to-end: started at ~2.16s, ended at ~0.68s. About 3.2× faster, all three checksums identical, no memory leaks.

Two more passes after that, both motivated by the next callgrind pass on the now-fast binary. The single hottest L1 write-miss line in the BFS hot loop was still `heatmap[next_index] += 1` — about 32% of all D1 write misses came from that one statement. `heatmap` was a `vector<int>`, and the max value seen anywhere in the workload is 957, so 16 bits is more than enough. Shrunk `heatmap` to `vector<uint16_t>`. Touched a handful of signatures (`shortest_path_bfs`, `run_all_requests`, `summarize_heatmap`, `compute_congestion_pressure`, `main`, plus the sanity check) but no semantic change. The same line in BFS now does a 2-byte RMW instead of 4, and `compute_congestion_pressure` reads half as many bytes when it pulls source values.

While I was inside `compute_congestion_pressure`, the `source` vector also looked redundant. It was a precomputed copy of `heatmap >> 3` that the inner loop read once per cell. Replaced it with `heatmap[index] >> 3` inline — saves the 270 KB allocation, the init pass over all 67,600 cells, and (combined with the `uint16_t` shrink) drops the source read from 4 bytes to 2 bytes per cell. The col loop still vectorizes as 32-byte AVX2 vectors after the change, confirmed via `-fopt-info-vec`. After both:

```
time_sec ≈ 0.71
```

Wall-clock barely moved (the inner loop is dominated by the five `cur_data` reads, not the one source read), but the memory footprint is meaningfully smaller and the code is a step cleaner — one fewer buffer to allocate and reason about.

A fresh `perf stat` after all that showed the program was now dominated by BFS (around 79% of wall time) with a branch-miss rate of ~13% and an IPC of only 1.18 — clearly memory- and branch-limited. The hottest line in BFS was still `if (grid[next_index] == '#')` at ~16% of all program instructions, and the second-hottest was `if (distance[next_index] != sentinel)` at ~8%. Together those two checks were a quarter of the entire program, and they're both data-dependent (random grid, BFS visit history) so the predictor can't learn them.

The fix: collapse the two checks into one by encoding "blocked" inside the distance array itself. Pre-fill `distance_buf` in `run_all_requests` so blocked cells start at `0xFFFE` and open cells start at `0xFFFF`. Then the BFS body only needs `if (distance[next_index] != 0xFFFF) continue;` — that rejects both blocked cells (`0xFFFE`) and previously-visited cells (any real distance). The grid array stops being read in the hot path entirely.

The per-call reset has to change with it. A plain `memset` would wipe the blocked sentinels. Two options: walk only the touched cells (those in `frontier`) and set them back to `0xFFFF` — a "touched-list" reset — or do a sequential pass over the full distance array, rederiving each cell from `grid[i]`. I tried both. The touched-list version is conceptually appealing because it only does O(visited) work per call, but it's a stream of random-indexed writes; the sequential rewrite does 2.5× more total work but the writes are linear, prefetcher-friendly, and AVX2-vectorizable. On this workload the sequential pass wins by about 10 ms median and is much more stable run-to-run. Kept the sequential version. After:

```
0.380 seconds time elapsed
```

The diff in `perf stat` is striking — same instruction count, but every other metric improved:

| metric | before | after |
|---|---|---|
| wall time | 0.571 s | **0.380 s** |
| cycles | 2.15 B | 1.53 B |
| IPC | 1.18 | **1.68** |
| branches | 331 M | 283 M |
| branch misses | 42.9 M (12.96%) | **23.6 M (8.35%)** |
| L1 D-cache loads | 636 M | 568 M |
| L1 D-cache load misses | 32.7 M (5.13%) | **8.3 M (1.47%)** |

L1 load misses dropped by **75%** — that's the grid array (67 KB) falling out of the BFS working set. Branch misses dropped by **45%** — one of the two data-dependent branches per neighbor is gone. IPC jumped from 1.18 to 1.68, meaning the CPU is finally finding parallelism in the inner loop now that there's less serial dependency on cache and predictor.

This is the biggest single optimization in the lab.

### Before — baseline

```
time_sec = 2.16284
       2.190593567 seconds time elapsed
       2.050109000 seconds user
       0.133876000 seconds sys
```

Callgrind totals on the baseline: `shortest_path_bfs` at 30.48% of instructions, `compute_congestion_pressure` at 19.99%.

### After — final

```
0.348 seconds time elapsed
0.348 seconds user
0.001 seconds sys
```

Same 1,200 route requests, same 4,096 congestion passes, same checksums. ~6× faster end-to-end (~2.16 s → ~0.35 s).

The final `perf stat` shows IPC at 1.68 (was 1.18), branch-miss rate at 8.35% (was 12.96%), and L1 D-cache miss rate at 1.47% (was 5.13%). The program is no longer memory- or branch-limited in the way it used to be. With BFS now reading a single array per neighbor and one of its two unpredictable branches gone, the inner loop runs close to what the front-end can dispatch. `compute_congestion_pressure` is bandwidth-bound on its three arrays (`current`, `next`, `heatmap`) and sits near the ceiling for what AVX2 can sustain on this CPU. The rest of the program (~2%) is one-time setup.


## 3. Correctness Evidence

`--test` passes after every step:

```
$ ./grid_bfs --test
sanity check passed
```

Full normal run on the optimized binary:

```
grid = 260 x 260
open_cells = 51260
requests = 1200
reachable = 1177
unreachable = 23
average_distance = 180.575
route_label_checksum = 3703473789245134517
heatmap_total_visits = 32914184
heatmap_active_cells = 51041
heatmap_max_visits = 957
heatmap_threshold_checksum = 17645577948039157950
congestion_passes = 4096
congestion_total_pressure = 3719781
congestion_max_pressure = 175
congestion_pressure_checksum = 5595025244828244209
time_sec = 0.584816
```

The three checksums (`route_label_checksum`, `heatmap_threshold_checksum`, `congestion_pressure_checksum`) are bit-identical to the unmodified program's checksums:

| Checksum | Unmodified | Optimized |
|---|---|---|
| `route_label_checksum` | 3703473789245134517 | 3703473789245134517 |
| `heatmap_threshold_checksum` | 17645577948039157950 | 17645577948039157950 |
| `congestion_pressure_checksum` | 5595025244828244209 | 5595025244828244209 |

Valgrind on the optimized build reports `definitely lost: 0 bytes in 0 blocks`. The original program leaked ~8 MB worth of `distance` and `visited` blocks per run (one pair per BFS call); converting those raw `new[]` allocations to `std::vector` in step 6 fixed the leak, and the hoist in step 12 means there are now only two allocations for the BFS buffers across the entire program, both freed cleanly at scope exit.

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