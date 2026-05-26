# Performance Assignment

Find the hotspots using any valid tool (primarily one of
perf/flamegraph/kcachegrind) that you see fit. Attach screenshots or text
outputs to an `Answers.md` file showcasing how the hotspot was found.

Try to improve the performance and compare.

Additionally, update `history_cols` to `2048` and see if new hotspots pop up.
If yes, fix them.

## Build And Run

```bash
make all
./main
```

You can also run:

```bash
make test
```

The baseline program with `history_cols = 128` should print:

```text
6040578838
```

When you change only `history_cols` to `2048`, the unoptimized starter program
should print:

```text
6745589558
```

Make sure the code gives the same output after all optimizations for whichever
`history_cols` value you are testing.

Do not modify any of the `build_` functions. They are not hotspots. Make sure
not to modify any function signatures or the call sites for those functions.
You may pass a different variable of the same type as an existing parameter
when calling a function.

This problem is more open ended. There are specific hotspots in the code, but
there is more than one way to fix each of them. Some fixes may be complicated,
and some hotspots may not be worth fixing. Decide which hotspot is the easiest
and most useful to fix at each step, then measure again.
