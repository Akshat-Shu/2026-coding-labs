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
__attribute__((noinline)) int to_index(int row, int col, int cols) {
    return row * cols + col;
}

/**
 * Return true if the coordinate is inside the grid bounds.
 */
__attribute__((noinline)) bool in_bounds(int row, int col, int rows, int cols) {
    return row >= 0 && row < rows && col >= 0 && col < cols;
}

/**
 * Return true if the coordinate refers to a traversable grid cell.
 */
__attribute__((noinline)) bool is_open(const vector<string> &grid, int row, int col) {
    return grid[row][col] != '#';
}

/**
 * Build a deterministic grid with open corridors and blocked cells.
 *
 * The pattern is generated in memory to keep the activity focused on CPU
 * profiling rather than file parsing or filesystem behavior.
 */
vector<string> generate_grid(int rows, int cols) {
    vector<string> grid(rows, string(cols, '.'));
    mt19937 rng(kSeed);
    uniform_int_distribution<int> percent(0, 99);

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            bool border = row == 0 || col == 0 || row == rows - 1 || col == cols - 1;
            bool corridor = (row % 17 == 1) || (col % 19 == 1);
            bool blocked = percent(rng) < 26;

            if (border) {
                grid[row][col] = '#';
            } else if (corridor) {
                grid[row][col] = '.';
            } else {
                grid[row][col] = blocked ? '#' : '.';
            }
        }
    }

    return grid;
}

/**
 * Find the next open cell while walking through the grid in row-major order.
 */
Point next_open_cell(const vector<string> &grid, int &cursor) {
    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());
    int total = rows * cols;

    for (int step = 0; step < total; ++step) {
        int index = (cursor + step) % total;
        int row = index / cols;
        int col = index % cols;

        if (is_open(grid, row, col)) {
            cursor = (index + 1) % total;
            return {row, col};
        }
    }

    return {1, 1};
}

/**
 * Generate deterministic route requests over open cells in the grid.
 */
vector<RouteRequest> generate_requests(const vector<string> &grid, int count) {
    vector<RouteRequest> requests;
    requests.reserve(count);

    int start_cursor = 0;
    int goal_cursor = static_cast<int>(grid.size() * grid[0].size()) / 2;

    for (int i = 0; i < count; ++i) {
        Point start = next_open_cell(grid, start_cursor);
        Point goal = next_open_cell(grid, goal_cursor);
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
int shortest_path_bfs(const vector<string> &grid, const RouteRequest &request,
                      vector<int> &heatmap) {
    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());
    int total = rows * cols;

    int *distance = new int[total];
    std::fill(distance, distance + total, -1);
    unsigned char *visited = new unsigned char[total]{};
    vector<Point> frontier(static_cast<size_t>(total));
    size_t frontier_head = 0;
    size_t frontier_tail = 0;

    int start_index = request.start.row * cols + request.start.col;
    int goal_index = request.goal.row * cols + request.goal.col;

    visited[start_index] = 1;
    distance[start_index] = 0;
    heatmap[start_index] += 1;
    frontier[frontier_tail++] = request.start;

    const int drow[4] = {-1, 1, 0, 0};
    const int dcol[4] = {0, 0, -1, 1};

    while (frontier_head < frontier_tail) {
        Point current = frontier[frontier_head++];

        int current_index = current.row * cols + current.col;
        if (current_index == goal_index) {
            return distance[current_index];
        }

        for (int direction = 0; direction < 4; ++direction) {
            int next_row = current.row + drow[direction];
            int next_col = current.col + dcol[direction];

            if (next_row < 0 || next_row >= rows || next_col < 0 || next_col >= cols) {
                continue;
            }
            if (grid[next_row][next_col] == '#') {
                continue;
            }

            int next_index = next_row * cols + next_col;
            if (visited[next_index]) {
                continue;
            }

            visited[next_index] = 1;
            distance[next_index] = distance[current_index] + 1;
            heatmap[next_index] += 1;
            frontier[frontier_tail++] = {next_row, next_col};
        }
    }

    return -1;
}

/**
 * Run all route requests and aggregate a compact summary.
 */
RunSummary run_all_requests(const vector<string> &grid,
                            const vector<RouteRequest> &requests,
                            vector<int> &heatmap) {
    RunSummary summary;
    summary.requests = static_cast<int>(requests.size());

    for (int i = 0; i < summary.requests; ++i) {
        const RouteRequest &request = requests[i];
        string route_label = format_route_label(request, i);
        summary.route_label_checksum ^= checksum_label(route_label);

        int distance = shortest_path_bfs(grid, request, heatmap);

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
HeatmapSummary summarize_heatmap(const vector<int> &heatmap, int rows, int cols) {
    HeatmapSummary summary;
    array<int, kRequestCount + 1> visit_counts{};

    for (int row = 0; row < rows; ++row) {
        int row_offset = row * cols;
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
int next_pressure_value(int center, int north, int south, int west, int east,
                        int source, int row, int col, int pass) {
    int pulse = (row * 17 + col * 31 + pass * 13) & 15;
    int pressure = (center * 2 + north + south + west + east + source + pulse) / 8;

    if (((center + row + pass) & 7) == 0) {
        pressure = pressure / 2 + source;
    } else {
        pressure += center & 3;
    }

    return min(pressure, 8191);
}

/**
 * Evolve the raw visit heatmap into a congestion-pressure map.
 *
 * Each pass depends on the previous pass, so the outer loop represents real
 * iterative work. The inner loops intentionally walk a row-major array in
 * column-major order to create a cache-locality problem for students to find.
 */
CongestionSummary compute_congestion_pressure(const vector<int> &heatmap,
                                              int rows, int cols,
                                              int congestion_passes) {
    vector<int> current = heatmap;
    vector<int> next = heatmap;
    vector<int> source(heatmap.size());

    for (size_t i = 0; i < heatmap.size(); ++i) {
        source[i] = heatmap[i] / 8;
    }

    for (int pass = 0; pass < congestion_passes; ++pass) {
        for (int col = 1; col < cols - 1; ++col) {
            for (int row = 1; row < rows - 1; ++row) {
                int index = row * cols + col;

                int center = current[index];
                int north = current[index - cols];
                int south = current[index + cols];
                int west = current[index - 1];
                int east = current[index + 1];

                next[index] = next_pressure_value(center, north, south, west, east,
                                                  source[index], row, col, pass);
            }
        }

        current.swap(next);
    }

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
 * Count open cells in the grid.
 */
int count_open_cells(const vector<string> &grid) {
    int open_cells = 0;

    for (const string &row : grid) {
        open_cells += static_cast<int>(count(row.begin(), row.end(), '.'));
    }

    return open_cells;
}

/**
 * Print the final summary in a stable, human-readable format.
 */
void print_summary(const vector<string> &grid,
                   const RunSummary &summary,
                   const HeatmapSummary &heatmap_summary,
                   const CongestionSummary &congestion_summary,
                   int congestion_passes,
                   double seconds) {
    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());
    int open_cells = count_open_cells(grid);

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
    vector<string> grid = {
        ".....",
        ".###.",
        "...#.",
        ".#...",
        ".....",
    };
    vector<int> heatmap(static_cast<int>(grid.size() * grid[0].size()), 0);

    RouteRequest reachable{{0, 0}, {4, 4}};
    RouteRequest unreachable{{0, 0}, {1, 1}};

    return shortest_path_bfs(grid, reachable, heatmap) == 8 &&
           shortest_path_bfs(grid, unreachable, heatmap) == -1;
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

    vector<string> grid = generate_grid(kRows, kCols);
    vector<RouteRequest> requests = generate_requests(grid, request_count);
    vector<int> heatmap(kRows * kCols, 0);
    RunSummary summary = run_all_requests(grid, requests, heatmap);
    HeatmapSummary heatmap_summary = summarize_heatmap(heatmap, kRows, kCols);
    CongestionSummary congestion_summary =
        compute_congestion_pressure(heatmap, kRows, kCols, congestion_passes);

    auto end = chrono::steady_clock::now();
    double seconds = chrono::duration<double>(end - start).count();

    print_summary(grid, summary, heatmap_summary, congestion_summary,
                  congestion_passes, seconds);
    return 0;
}
