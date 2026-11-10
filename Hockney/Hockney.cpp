#include <mpi.h>
#include <iostream>
#include <vector>
#include <iomanip>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        if (rank == 0) {
            std::cerr << "Error: Ping-pong exchange requires at least 2 processes.\n";
        }
        MPI_Finalize();
        return 1;
    }

    // Кількість повторень n та максимальний розмір повідомлення (4 МБ)
    const int ITERS = 10000;
    const int MIN_MSG_SIZE = 8;
    const int MAX_MSG_SIZE = 4 * 1024 * 1024;

    std::vector<char> send_buf(MAX_MSG_SIZE, 'a');
    std::vector<char> recv_buf(MAX_MSG_SIZE, 'b');

    if (rank == 0) {
        std::cout << "Size(bytes),Transfer_Time(s),Bandwidth(MB/s)\n";
    }

    double alpha_estimate = 0.0;
    double t_min = 0.0, t_max = 0.0;

    for (int m = MIN_MSG_SIZE; m <= MAX_MSG_SIZE; m *= 2) {
        MPI_Barrier(MPI_COMM_WORLD);
        double start_time = MPI_Wtime();

        for (int i = 0; i < ITERS; ++i) {
            if (rank == 0) {
                MPI_Sendrecv(send_buf.data(), m, MPI_CHAR, 1, 0,
                    recv_buf.data(), m, MPI_CHAR, 1, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            else if (rank == 1) {
                MPI_Sendrecv(send_buf.data(), m, MPI_CHAR, 0, 0,
                    recv_buf.data(), m, MPI_CHAR, 0, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }

        double end_time = MPI_Wtime();
        double total_time = end_time - start_time;
        double time_per_msg = total_time / ITERS;

        if (rank == 0) {
            double bandwidth_mb = (m / (1024.0 * 1024.0)) / time_per_msg;
            std::cout << m << "," << std::scientific << time_per_msg << ","
                << std::fixed << std::setprecision(2) << bandwidth_mb << "\n";

            // За малих m час наближається до латентності
            if (m == MIN_MSG_SIZE) {
                alpha_estimate = time_per_msg;
                t_min = time_per_msg;
            }
            if (m == MAX_MSG_SIZE) {
                t_max = time_per_msg;
            }
        }
    }

    if (rank == 0) {
        // Пропускна здатність визначається за нахилом залежності за великих m
        double beta_estimate = (double)(MAX_MSG_SIZE - MIN_MSG_SIZE) / (t_max - t_min);

        std::cout << "\nHockney Model Parameters Estimation\n";
        std::cout << "Latency (alpha): " << std::scientific << alpha_estimate << " s\n";
        std::cout << "Bandwidth (beta): " << std::fixed << std::setprecision(2) << beta_estimate << " bytes/s\n";
        std::cout << "Bandwidth (beta): " << std::fixed << std::setprecision(2) << beta_estimate / (1024.0 * 1024.0) << " MB/s\n";
    }

    MPI_Finalize();
    return 0;
}