CXX := g++
CXXFLAGS_BASE := -std=c++20 -Wall -Wextra -pedantic
CXXFLAGS_RELEASE := $(CXXFLAGS_BASE) -O2 -g
CXXFLAGS_DEBUG := $(CXXFLAGS_BASE) -O0 -g
CXXFLAGS_PERF := $(CXXFLAGS_BASE) -O2 -g -fno-omit-frame-pointer
CXXFLAGS_GPROF := $(CXXFLAGS_BASE) -O0 -g -pg
CXXFLAGS_OPTIMIZED := $(CXXFLAGS_BASE) -O3 -g

TARGET := grid_bfs
SOURCE := grid_bfs.cpp

.PHONY: all debug perf gprof optimized test benchmark clean help

all: $(SOURCE)
	$(CXX) $(CXXFLAGS_RELEASE) $< -o $(TARGET)

debug: $(SOURCE)
	$(CXX) $(CXXFLAGS_DEBUG) $< -o $(TARGET)

perf: $(SOURCE)
	$(CXX) $(CXXFLAGS_PERF) $< -o $(TARGET)

gprof: $(SOURCE)
	$(CXX) $(CXXFLAGS_GPROF) $< -o $(TARGET)

optimized: $(SOURCE)
	$(CXX) $(CXXFLAGS_OPTIMIZED) $< -o $(TARGET)

test: all
	./$(TARGET) --test

benchmark: all
	@echo "Run 1"
	@/usr/bin/time -f "elapsed=%e user=%U sys=%S" ./$(TARGET) >/dev/null
	@echo "Run 2"
	@/usr/bin/time -f "elapsed=%e user=%U sys=%S" ./$(TARGET) >/dev/null
	@echo "Run 3"
	@/usr/bin/time -f "elapsed=%e user=%U sys=%S" ./$(TARGET) >/dev/null

clean:
	rm -f $(TARGET) gmon.out perf.data perf.data.old perf.script perf.folded flamegraph.svg
	rm -f cachegrind.out* callgrind.out*

help:
	@echo "Targets:"
	@echo "  all        - build with -O2 -g"
	@echo "  debug      - build with -O0 -g"
	@echo "  perf       - build with frame pointers for perf call stacks"
	@echo "  gprof      - build with -O0 -g -pg for gprof"
	@echo "  optimized  - build with -O3 -g"
	@echo "  test       - run deterministic sanity check"
	@echo "  benchmark  - time the fixed workload three times"
	@echo "  clean      - remove binary and profiler outputs"
