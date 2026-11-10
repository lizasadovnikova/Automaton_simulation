#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>

const char EMPTY = 0;
const char TREE = 1;
const char BURNING = 2;
const char BURNED = 3;

bool has_tree(int seed, size_t i, size_t j, double d) {
    unsigned int h = (unsigned int)seed;
    h ^= i; h *= 0x5bd1e995;
    h ^= j; h *= 0x5bd1e995;
    h ^= h >> 15;
    return ((double)(h & 0xFFFFFF) / (double)0xFFFFFF) < d;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    size_t N = 1000;
    double d = 0.60;
    int seed = 42;
    int exchange_method = 0; // 0 - Тупик, 1 - Упорядкування, 2 - Неблокувальний

    if (argc > 1) N = std::stoull(argv[1]);
    if (argc > 2) exchange_method = std::atoi(argv[2]);

    size_t local_start = rank * (N / size);
    size_t local_end = (rank == size - 1) ? N : (rank + 1) * (N / size);
    size_t local_rows = local_end - local_start;

    std::vector<char> grid((local_rows + 2) * N, EMPTY);
    std::vector<char> next_grid((local_rows + 2) * N, EMPTY);

    long long local_initial_burning = 0;

    for (size_t i = 0; i < local_rows; ++i) {
        size_t global_i = local_start + i;
        for (size_t j = 0; j < N; ++j) {
            if (has_tree(seed, global_i, j, d)) {
                grid[(i + 1) * N + j] = TREE;
                if (j == 0) {
                    grid[(i + 1) * N + j] = BURNING;
                    local_initial_burning++;
                }
            }
        }
    }

    long long steps = 0;
    long long total_burned = 0;
    long long global_total_burning = 0;

    MPI_Allreduce(&local_initial_burning, &global_total_burning, 1, MPI_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);

    double total_exchange_time = 0.0;

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    while (global_total_burning > 0) {
        long long newly_ignited = 0;
        int top = (rank == 0) ? MPI_PROC_NULL : rank - 1;
        int bottom = (rank == size - 1) ? MPI_PROC_NULL : rank + 1;

        double comm_start = MPI_Wtime();

        if (exchange_method == 0) {
            // ТУПИК: Спочатку два блокувальні Send, потім два Recv
            MPI_Send(grid.data() + N, N, MPI_CHAR, top, 0, MPI_COMM_WORLD);
            MPI_Send(grid.data() + local_rows * N, N, MPI_CHAR, bottom, 1, MPI_COMM_WORLD);
            MPI_Recv(grid.data(), N, MPI_CHAR, top, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(grid.data() + (local_rows + 1) * N, N, MPI_CHAR, bottom, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        else if (exchange_method == 1) {
            // СПОСІБ A: Упорядкування операцій
            MPI_Sendrecv(grid.data() + N, N, MPI_CHAR, top, 0,
                grid.data() + (local_rows + 1) * N, N, MPI_CHAR, bottom, 0,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            MPI_Sendrecv(grid.data() + local_rows * N, N, MPI_CHAR, bottom, 1,
                grid.data(), N, MPI_CHAR, top, 1,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        else if (exchange_method == 2) {
            // СПОСІБ B: Неблокувальний обмін
            MPI_Request reqs[4];
            int req_count = 0;

            if (top != MPI_PROC_NULL) {
                MPI_Irecv(grid.data(), N, MPI_CHAR, top, 1, MPI_COMM_WORLD, &reqs[req_count++]);
            }
            if (bottom != MPI_PROC_NULL) {
                MPI_Irecv(grid.data() + (local_rows + 1) * N, N, MPI_CHAR, bottom, 0, MPI_COMM_WORLD, &reqs[req_count++]);
            }

            if (top != MPI_PROC_NULL) {
                MPI_Isend(grid.data() + N, N, MPI_CHAR, top, 0, MPI_COMM_WORLD, &reqs[req_count++]);
            }
            if (bottom != MPI_PROC_NULL) {
                MPI_Isend(grid.data() + local_rows * N, N, MPI_CHAR, bottom, 1, MPI_COMM_WORLD, &reqs[req_count++]);
            }

            if (req_count > 0) {
                MPI_Waitall(req_count, reqs, MPI_STATUSES_IGNORE);
            }
        }

        total_exchange_time += (MPI_Wtime() - comm_start);

        for (size_t i = 1; i <= local_rows; ++i) {
            for (size_t j = 0; j < N; ++j) {
                size_t idx = i * N + j;
                next_grid[idx] = grid[idx];

                if (grid[idx] == BURNING) {
                    next_grid[idx] = BURNED;
                    total_burned++;
                }
                else if (grid[idx] == TREE) {
                    if ((grid[(i - 1) * N + j] == BURNING) ||
                        (grid[(i + 1) * N + j] == BURNING) ||
                        (j > 0 && grid[idx - 1] == BURNING) ||
                        (j < N - 1 && grid[idx + 1] == BURNING)) {

                        next_grid[idx] = BURNING;
                        newly_ignited++;
                    }
                }
            }
        }
        grid.swap(next_grid);
        steps++;

        MPI_Allreduce(&newly_ignited, &global_total_burning, 1, MPI_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
    }

    double local_duration = MPI_Wtime() - start_time;
    double max_duration;
    MPI_Reduce(&local_duration, &max_duration, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    double max_exchange_time;
    MPI_Reduce(&total_exchange_time, &max_exchange_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::string method_name = (exchange_method == 0) ? "Deadlock" : ((exchange_method == 1) ? "Method A (Sendrecv)" : "Method B (Non-blocking)");
        std::cout << "V2 Exploration of a deadlock\n";
        std::cout << "N: " << N << ", P: " << size << ", Method: " << method_name << "\n";
        std::cout << "Steps taken: " << steps << "\n";
        std::cout << "Execution time: " << max_duration << " sec\n";
        std::cout << "Exchange time : " << max_exchange_time << " sec\n";
    }

    MPI_Finalize();
    return 0;
}