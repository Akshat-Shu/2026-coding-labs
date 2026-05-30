#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <vector>

using namespace std;

constexpr int kRows = 260;
constexpr int kCols = 260;
constexpr int kRequestCount = 1200;
constexpr int kSmallRequestCount = 25;
constexpr int kHeatmapThresholdCount = 256;
constexpr int kCongestionPasses = 4096;
constexpr int kSmallCongestionPasses = 32;
constexpr uint32_t kSeed = 0xC0FFEEu;

struct Point {
    int row;
    int col;
};

struct RouteRequest {
    Point start;
    Point goal;
};

struct RunSummary {
    int requests = 0;
    int reachable = 0;
    int unreachable = 0;
    long long total_distance = 0;
    uint64_t route_label_checksum = 0;
};

struct HeatmapSummary {
    long long total_visits = 0;
    int active_cells = 0;
    int max_visits = 0;
    uint64_t threshold_checksum = 0;
};

struct CongestionSummary {
    long long total_pressure = 0;
    int max_pressure = 0;
    uint64_t pressure_checksum = 0;
};

/**
 * Convert a row and column pair into a one-dimensional array index.
 */
inline int to_index(int row, int col, int cols) {
    return row * cols + col;
}

/**
 * Return true if the coordinate is inside the grid bounds.
 */
inline bool in_bounds(int row, int col, int rows, int cols) {
    return row >= 0 && row < rows && col >= 0 && col < cols;
}

/**
 * Return true if the coordinate refers to a traversable grid cell.
 */
inline bool is_open(const char *grid, int row, int col, int cols) {
    return grid[row * cols + col] != '#';
}

/**
 * Build a deterministic grid with open corridors and blocked cells.
 *
 * The pattern is generated in memory to keep the activity focused on CPU
 * profiling rather than file parsing or filesystem behavior.
 */
void generate_grid(char *grid, int rows, int cols) {
    mt19937 rng(kSeed);
    uniform_int_distribution<int> percent(0, 99);

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            bool border = row == 0 || col == 0 || row == rows - 1 || col == cols - 1;
            bool corridor = (row % 17 == 1) || (col % 19 == 1);
            bool blocked = percent(rng) < 26;

            char cell;
            if (border) {
                cell = '#';
            } else if (corridor) {
                cell = '.';
            } else {
                cell = blocked ? '#' : '.';
            }
            grid[row * cols + col] = cell;
        }
    }
}

/**
 * Find the next open cell while walking through the grid in row-major order.
 */
Point next_open_cell(const char *grid, int rows, int cols, int &cursor) {
    int total = rows * cols;

    for (int step = 0; step < total; ++step) {
        int index = (cursor + step) % total;
        int row = index / cols;
        int col = index % cols;

        if (is_open(grid, row, col, cols)) {
            cursor = (index + 1) % total;
            return {row, col};
        }
    }

    return {1, 1};
}

/**
 * Generate deterministic route requests over open cells in the grid.
 */
vector<RouteRequest> generate_requests(const char *grid, int rows, int cols, int count) {
    vector<RouteRequest> requests;
    requests.reserve(count);

    int start_cursor = 0;
    int goal_cursor = (rows * cols) / 2;

    for (int i = 0; i < count; ++i) {
        Point start = next_open_cell(grid, rows, cols, start_cursor);
        Point goal = next_open_cell(grid, rows, cols, goal_cursor);
        requests.push_back({start, goal});

        start_cursor += 37 + (i % 11);
        goal_cursor += 91 + (i % 17);
    }

    return requests;
}

/**
 * Build a short human-readable label for one route request.
 *
 * The label is folded into a checksum so that the work remains visible to
 * profilers and is not discarded as dead code.
 */
string format_route_label(const RouteRequest &request, int request_index) {
    string label = "route:";
    label += to_string(request_index);
    label += ":";
    label += to_string(request.start.row);
    label += ",";
    label += to_string(request.start.col);
    label += "->";
    label += to_string(request.goal.row);
    label += ",";
    label += to_string(request.goal.col);
    return label;
}

/**
 * Fold a label string into a simple deterministic checksum.
 */
uint64_t checksum_label(const string &label) {
    uint64_t checksum = 1469598103934665603ULL;

    for (unsigned char ch : label) {
        checksum ^= ch;
        checksum *= 1099511628211ULL;
    }

    return checksum;
}

/**
 * Compute the shortest path length between two points using BFS.
 *
 * The function returns -1 when the goal cannot be reached.
 */
int shortest_path_bfs(const char *grid, int rows, int cols,
                      const RouteRequest &request,
                      vector<uint16_t> &heatmap,
                      vector<uint16_t> &distance,
                      vector<int> &frontier) {
    int total = rows * cols;
    size_t frontier_head = 0;
    size_t frontier_tail = 0;

    int start_index = request.start.row * cols + request.start.col;
    int goal_index = request.goal.row * cols + request.goal.col;

    distance[start_index] = 0;
    heatmap[start_index] += 1;
    frontier[frontier_tail++] = start_index;

    const int offsets[4] = {-cols, cols, -1, 1};

    int result = -1;
    while (frontier_head < frontier_tail) {
        int current_index = frontier[frontier_head++];
        if (current_index == goal_index) {
            result = distance[current_index];
            break;
        }

        for (int direction = 0; direction < 4; ++direction) {
            int next_index = current_index + offsets[direction];

            // 0xFFFE = blocked or visited. Only 0xFFFF (open, unvisited) passes.
            if (distance[next_index] != static_cast<uint16_t>(-1)) {
                continue;
            }

            distance[next_index] = distance[current_index] + 1;
            heatmap[next_index] += 1;
            frontier[frontier_tail++] = next_index;
        }
    }


    for (int i = 0; i < total; ++i) {
        distance[i] = (grid[i] == '#') ? static_cast<uint16_t>(-2)
                                       : static_cast<uint16_t>(-1);
    }
    return result;
}

/**
 * Run all route requests and aggregate a compact summary.
 */
RunSummary run_all_requests(const char *grid, int rows, int cols,
                            const vector<RouteRequest> &requests,
                            vector<uint16_t> &heatmap) {
    RunSummary summary;
    summary.requests = static_cast<int>(requests.size());

    int total = rows * cols;
    vector<uint16_t> distance_buf(total);
    vector<int> frontier_buf(total);

    // Encode "blocked" into the distance sentinel: 0xFFFE = blocked, 0xFFFF = open & unvisited.
    // BFS only needs to check `distance[i] != 0xFFFF` then — blocked and visited both fail.
    for (int i = 0; i < total; ++i) {
        distance_buf[i] = (grid[i] == '#') ? static_cast<uint16_t>(-2)
                                           : static_cast<uint16_t>(-1);
    }

    for (int i = 0; i < summary.requests; ++i) {
        const RouteRequest &request = requests[i];
        string route_label = format_route_label(request, i);
        summary.route_label_checksum ^= checksum_label(route_label);

        int distance = shortest_path_bfs(grid, rows, cols, request, heatmap,
                                         distance_buf, frontier_buf);

        if (distance >= 0) {
            summary.reachable += 1;
            summary.total_distance += distance;
        } else {
            summary.unreachable += 1;
        }
    }

    return summary;
}

/**
 * Summarize how often the BFS workload touched each cell.
 *
 * The function computes basic totals and a cumulative visit-distribution
 * checksum that the final report can print.
 */
HeatmapSummary summarize_heatmap(const vector<uint16_t> &heatmap, int rows, int cols) {
    HeatmapSummary summary;
    array<int, kRequestCount + 1> visit_counts{};

    for (int row = 0; row < rows; ++row) {
        const int row_offset = row * cols;
        for (int col = 0; col < cols; ++col) {
            int visits = heatmap[row_offset + col];
            summary.total_visits += visits;

            if (visits > 0) {
                summary.active_cells += 1;
            }
            if (visits > summary.max_visits) {
                summary.max_visits = visits;
            }
            if (visits >= 0 && visits < static_cast<int>(visit_counts.size())) {
                visit_counts[visits] += 1;
            }
        }
    }

    for (int threshold = 1; threshold <= kHeatmapThresholdCount; ++threshold) {
        int cells_at_or_above_threshold = 0;

        for (int visits = threshold; visits <= summary.max_visits; ++visits) {
            cells_at_or_above_threshold += visit_counts[visits];
        }

        summary.threshold_checksum =
            summary.threshold_checksum * 1315423911ULL +
            static_cast<uint64_t>(cells_at_or_above_threshold + threshold);
    }

    return summary;
}

/**
 * Compute one cell's next congestion-pressure value.
 *
 * The formula mixes the current cell, its four neighbors, the original heatmap
 * source value, and a small deterministic pulse so each pass keeps doing real
 * work instead of collapsing into a trivial copy.
 */
inline int next_pressure_value(int center, int north, int south, int west, int east,
                        int source, int row, int col, int pass) {
    int pulse = (row - col - 3 * pass) & 15;
    int pressure = ((center << 1) + north + south + west + east + source + pulse) >> 3;

    int sel  = (((center + row + pass) & 7) == 0);
    pressure = sel ? (pressure >> 1) + source : pressure + (center & 3);
    return pressure > 8191 ? 8191 : pressure;
}

__attribute__((always_inline)) inline CongestionSummary generate_congestion_summary(const vector<int>& current) {
    CongestionSummary summary;
    for (int value : current) {
        summary.total_pressure += value;
        if (value > summary.max_pressure) {
            summary.max_pressure = value;
        }
        summary.pressure_checksum =
            summary.pressure_checksum * 16777619ULL + static_cast<uint64_t>(value + 97);
    }

    return summary;
}

/**
 * Evolve the raw visit heatmap into a congestion-pressure map.
 *
 * Each pass depends on the previous pass, so the outer loop represents real
 * iterative work. The inner loops intentionally walk a row-major array in
 * column-major order to create a cache-locality problem for students to find.
 */
CongestionSummary compute_congestion_pressure(const vector<uint16_t> &heatmap,
                                              int rows, int cols,
                                              int congestion_passes) {
    vector<int> current(heatmap.begin(), heatmap.end());
    vector<int> next(heatmap.begin(), heatmap.end());

    for (int pass = 0; pass < congestion_passes; ++pass) {
        const int*      __restrict__ cur_data = current.data();
        const uint16_t* __restrict__ src_data = heatmap.data();
        int*            __restrict__ nxt_data = next.data();

        for (int row = 1; row < rows - 1; ++row) {
            const int row_offset = row * cols;
            for (int col = 1; col < cols - 1; ++col) {
                int index = row_offset + col;

                int center = cur_data[index];
                int north = cur_data[index - cols];
                int south = cur_data[index + cols];
                int west = cur_data[index - 1];
                int east = cur_data[index + 1];

                nxt_data[index] = next_pressure_value(center, north, south, west, east,
                                                     src_data[index] >> 3, row, col, pass);
            }
        }

        current.swap(next);
    }

    return generate_congestion_summary(current);
}

/**
 * Count open cells in the grid.
 */
int count_open_cells(const char *grid, int rows, int cols) {
    int open_cells = 0;
    int total = rows * cols;
    for (int i = 0; i < total; ++i) {
        if (grid[i] == '.') {
            open_cells += 1;
        }
    }
    return open_cells;
}

/**
 * Print the final summary in a stable, human-readable format.
 */
void print_summary(const char *grid, int rows, int cols,
                   const RunSummary &summary,
                   const HeatmapSummary &heatmap_summary,
                   const CongestionSummary &congestion_summary,
                   int congestion_passes,
                   double seconds) {
    int open_cells = count_open_cells(grid, rows, cols);

    double average_distance = 0.0;
    if (summary.reachable > 0) {
        average_distance = static_cast<double>(summary.total_distance) / summary.reachable;
    }

    cout << "grid = " << rows << " x " << cols << '\n';
    cout << "open_cells = " << open_cells << '\n';
    cout << "requests = " << summary.requests << '\n';
    cout << "reachable = " << summary.reachable << '\n';
    cout << "unreachable = " << summary.unreachable << '\n';
    cout << "average_distance = " << average_distance << '\n';
    cout << "route_label_checksum = " << summary.route_label_checksum << '\n';
    cout << "heatmap_total_visits = " << heatmap_summary.total_visits << '\n';
    cout << "heatmap_active_cells = " << heatmap_summary.active_cells << '\n';
    cout << "heatmap_max_visits = " << heatmap_summary.max_visits << '\n';
    cout << "heatmap_threshold_checksum = " << heatmap_summary.threshold_checksum << '\n';
    cout << "congestion_passes = " << congestion_passes << '\n';
    cout << "congestion_total_pressure = " << congestion_summary.total_pressure << '\n';
    cout << "congestion_max_pressure = " << congestion_summary.max_pressure << '\n';
    cout << "congestion_pressure_checksum = " << congestion_summary.pressure_checksum << '\n';
    cout << "time_sec = " << seconds << '\n';
}

/**
 * Run a tiny deterministic correctness check for BFS.
 */
bool run_sanity_check() {
    static constexpr int sanity_rows = 7;
    static constexpr int sanity_cols = 7;
    static const char sanity_grid[sanity_rows * sanity_cols] = {
        '#','#','#','#','#','#','#',
        '#','.','.','.','.','.','#',
        '#','.','#','#','#','.','#',
        '#','.','.','.','#','.','#',
        '#','.','#','.','.','.','#',
        '#','.','.','.','.','.','#',
        '#','#','#','#','#','#','#',
    };
    vector<uint16_t> heatmap(sanity_rows * sanity_cols, 0);
    vector<uint16_t> distance_buf(sanity_rows * sanity_cols);
    vector<int> frontier_buf(sanity_rows * sanity_cols);

    for (int i = 0; i < sanity_rows * sanity_cols; ++i) {
        distance_buf[i] = (sanity_grid[i] == '#') ? static_cast<uint16_t>(-2)
                                                  : static_cast<uint16_t>(-1);
    }

    RouteRequest reachable{{1, 1}, {5, 5}};
    RouteRequest unreachable{{1, 1}, {2, 2}};

    return shortest_path_bfs(sanity_grid, sanity_rows, sanity_cols, reachable, heatmap,
                             distance_buf, frontier_buf) == 8 &&
           shortest_path_bfs(sanity_grid, sanity_rows, sanity_cols, unreachable, heatmap,
                             distance_buf, frontier_buf) == -1;
}

/**
 * Entry point for the sanity check, small workload, or full workload.
 */
int main(int argc, char **argv) {
    if (argc == 2 && string(argv[1]) == "--test") {
        if (!run_sanity_check()) {
            cerr << "sanity check failed\n";
            return 1;
        }

        cout << "sanity check passed\n";
        return 0;
    }

    bool small_workload = argc == 2 && string(argv[1]) == "--small";
    if (argc != 1 && !small_workload) {
        cerr << "usage: " << argv[0] << " [--test|--small]\n";
        return 1;
    }

    int request_count = small_workload ? kSmallRequestCount : kRequestCount;
    int congestion_passes =
        small_workload ? kSmallCongestionPasses : kCongestionPasses;

    auto start = chrono::steady_clock::now();

    static std::array<char, kRows * kCols> grid;
    generate_grid(grid.data(), kRows, kCols);
    vector<RouteRequest> requests = generate_requests(grid.data(), kRows, kCols, request_count);
    vector<uint16_t> heatmap(kRows * kCols, 0);
    RunSummary summary = run_all_requests(grid.data(), kRows, kCols, requests, heatmap);
    HeatmapSummary heatmap_summary = summarize_heatmap(heatmap, kRows, kCols);
    CongestionSummary congestion_summary =
        compute_congestion_pressure(heatmap, kRows, kCols, congestion_passes);

    auto end = chrono::steady_clock::now();
    double seconds = chrono::duration<double>(end - start).count();

    print_summary(grid.data(), kRows, kCols, summary, heatmap_summary, congestion_summary,
                  congestion_passes, seconds);
    return 0;
}
