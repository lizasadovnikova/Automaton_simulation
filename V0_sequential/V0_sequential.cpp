#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>

// V0: Послідовна версія клітинного автомата
const char EMPTY = 0;
const char TREE = 1;
const char BURNING = 2;
const char BURNED = 3;

bool has_tree(int seed, int i, int j, double d) {
    unsigned int h = (unsigned int)seed;
    h ^= i; h *= 0x5bd1e995;
    h ^= j; h *= 0x5bd1e995;
    h ^= h >> 15;
    double val = (double)(h & 0xFFFFFF) / (double)0xFFFFFF;
    return val < d;
}

int main(int argc, char** argv) {
    size_t N = 1000;
    double d = 0.60;
    int seed = 42;
    if (argc > 1) N = std::stoull(argv[1]);
    if (argc > 2) d = std::stod(argv[2]);
    if (argc > 3) seed = std::stoi(argv[3]);

    std::vector<char> grid(N * N, EMPTY);
    std::vector<char> next_grid(N * N, EMPTY);

    long long steps = 0;
    long long total_burned = 0;
    int reached_right_edge = 0;
    long long burning_count = 0;

    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
            if (has_tree(seed, i, j, d)) {
                grid[i * N + j] = TREE;
                if (j == 0) {
                    grid[i * N + j] = BURNING;
                    burning_count++;
                }
            }
        }
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    while (burning_count > 0) {
        long long newly_ignited = 0;

        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                size_t idx = i * N + j;
                next_grid[idx] = grid[idx];

                if (grid[idx] == BURNING) {
                    next_grid[idx] = BURNED;
                    total_burned++;
                    if (j == N - 1) reached_right_edge = 1;
                }
                else if (grid[idx] == TREE) {
                    bool ignite = false;
                    if (i > 0 && grid[(i - 1) * N + j] == BURNING) ignite = true;
                    if (i < N - 1 && grid[(i + 1) * N + j] == BURNING) ignite = true;
                    if (j > 0 && grid[idx - 1] == BURNING) ignite = true;
                    if (j < N - 1 && grid[idx + 1] == BURNING) ignite = true;

                    if (ignite) {
                        next_grid[idx] = BURNING;
                        newly_ignited++;
                    }
                }
            }
        }
        grid.swap(next_grid);
        steps++;
        burning_count = newly_ignited;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    std::ofstream outfile("v0_final_state.bin", std::ios::binary);
    if (outfile) {
        outfile.write(grid.data(), grid.size());
        outfile.close();
        std::cout << "The final state of the grid has been saved to the file v0_final_state.bin\n";
    }
    else {
        std::cerr << "Error writing file v0_final_state.bin\n";
    }

    std::cout << "CSV_RESULT:" << N << "," << steps << "," << total_burned << "," << reached_right_edge << "," << duration.count() << "\n";

    std::cout << "V0 Sequential version\n";
    std::cout << "N: " << N << ", Steps: " << steps << ", Burned: " << total_burned << "\n";
    std::cout << "Reached the right edge: " << (reached_right_edge ? "Yes" : "No") << "\n";
    std::cout << "Execution time(base) : " << duration.count() << " sec\n";

    return 0;
}