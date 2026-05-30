# Profiling Lab Report

## 1. Optimizations Made

1. Rewrote the math in `next_pressure_value`. Pulse is now `(row - col - 3*pass) & 15` instead of `(row*17 + col*31 + pass*13) & 15` — same result mod 16, one multiply instead of three. Also swapped `*2`, `/8`, `/2` for `<<1`, `>>3`, `>>1`, and changed `heatmap[i] / 8` to `heatmap[i] >> 3` in `compute_congestion_pressure`. Divides showed up as the per-line hotspot in KCachegrind's annotation of `next_pressure_value`.

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
time_sec ≈ 1.49
       1.497640934 seconds time elapsed
       1.489401000 seconds user
       0.005993000 seconds sys
```

Down ~30%. Output checksums match the original, so the bit-twiddling didn't change the result.

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
time_sec ≈ 1.49
       1.497640934 seconds time elapsed
       1.489401000 seconds user
       0.005993000 seconds sys
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