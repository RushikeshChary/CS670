#include "shares.h"

using boost::asio::ip::tcp;

// note: function names and helpers are updated to match the renamed shares.h API
boost::asio::awaitable<void> handle_client(tcp::socket socket, std::string name,
                                           std::vector<std::vector<long long>> &x_k, std::vector<std::vector<long long>> &y_k,
                                           std::vector<long long> gamma_k, std::vector<std::vector<long long>> &x_n,
                                           std::vector<std::vector<long long>> &y_n, std::vector<long long> gamma_n,
                                           std::vector<long long> scalar_x, std::vector<long long> scalar_y,
                                           std::vector<long long> scalar_gamma, std::vector<std::vector<long long>> &r,
                                           std::vector<long long> alpha, int q, std::vector<std::vector<long long>> &key_vec) {
    long long party_id = (name == "P0") ? 0 : 1;
    co_await async_send_scalar(socket, party_id); // send party id to client
    long long size = static_cast<long long>(key_vec[0].size());
    co_await async_send_scalar(socket, size);

    for (int i = 0; i < q; i++) { // for each query, send new sets of du-atallah shares
        co_await async_send_vector1d(socket, key_vec[i]);

        co_await async_send_vector1d(socket, r[i]); // send shares for rotation trick
        co_await async_send_scalar(socket, alpha[i]);

        co_await async_send_vector1d(socket, x_k[i]);
        co_await async_send_vector1d(socket, y_k[i]);
        co_await async_send_scalar(socket, gamma_k[i]);

        co_await async_send_vector1d(socket, x_n[i]);
        co_await async_send_vector1d(socket, y_n[i]);
        co_await async_send_scalar(socket, gamma_n[i]);

        co_await async_send_scalar(socket, scalar_x[i]);
        co_await async_send_scalar(socket, scalar_y[i]);
        co_await async_send_scalar(socket, scalar_gamma[i]);
    }
}

// Run multiple coroutines in parallel
template <typename... Fs>
void run_in_parallel(boost::asio::io_context& io, Fs&&... funcs) {
    (boost::asio::co_spawn(io, funcs, boost::asio::detached), ...);
}

int main() {
    try {
        std::string query_file = "/app/files/input1.txt";
        std::ifstream ifs(query_file);
        if (!ifs) {
            std::cerr << "Error opening file for reading: " << query_file << std::endl;
            return 1;
        }
        int n, m, k, q;
        ifs >> n >> m >> k >> q;

        // k-dimension vectors for k-size du-atallah
        std::vector<std::vector<long long>> x0_k(q, std::vector<long long>(k)), x1_k(q, std::vector<long long>(k)),
                                          y0_k(q, std::vector<long long>(k)), y1_k(q, std::vector<long long>(k));
        std::vector<long long> alpha_k(q), gamma0_k(q), gamma1_k(q);
        for (int i = 0; i < q; i++) alpha_k[i] = randInt();
        fill_random_matrix(x0_k);
        fill_random_matrix(x1_k);
        fill_random_matrix(y0_k);
        fill_random_matrix(y1_k);
        for (int i = 0; i < q; i++) {
            gamma0_k[i] = vec_dot(x0_k[i], y1_k[i]) + alpha_k[i];
            gamma1_k[i] = vec_dot(x1_k[i], y0_k[i]) - alpha_k[i];
        }

        // n-dimension vectors for n-size du-atallah
        std::vector<std::vector<long long>> x0_n(q, std::vector<long long>(n)), x1_n(q, std::vector<long long>(n)),
                                          y0_n(q, std::vector<long long>(n)), y1_n(q, std::vector<long long>(n));
        std::vector<long long> alpha_n(q), gamma0_n(q), gamma1_n(q);
        for (int i = 0; i < q; i++) alpha_n[i] = randInt();
        fill_random_matrix(x0_n);
        fill_random_matrix(x1_n);
        fill_random_matrix(y0_n);
        fill_random_matrix(y1_n);
        for (int i = 0; i < q; i++) {
            gamma0_n[i] = vec_dot(x0_n[i], y1_n[i]) + alpha_n[i];
            gamma1_n[i] = vec_dot(x1_n[i], y0_n[i]) - alpha_n[i];
        }

        // scalar du-atallah
        std::vector<long long> scalar_x0(q), scalar_x1(q), scalar_y0(q), scalar_y1(q), scalar_alpha(q), scalar_gamma0(q), scalar_gamma1(q);
        for (int i = 0; i < q; i++) {
            scalar_x0[i] = randInt();
            scalar_x1[i] = randInt();
            scalar_y0[i] = randInt();
            scalar_y1[i] = randInt();
            scalar_alpha[i] = randInt();
            scalar_gamma0[i] = scalar_x0[i] * scalar_y1[i] + scalar_alpha[i];
            scalar_gamma1[i] = scalar_x1[i] * scalar_y0[i] - scalar_alpha[i];
        }

        // rotation trick helpers
        std::vector<long long> e_alpha(q);
        for (int i = 0; i < q; i++) e_alpha[i] = randInt(0, n - 1);
        std::vector<long long> alpha_0(q), alpha_1(q);
        for (int i = 0; i < q; i++) {
            alpha_0[i] = randInt();
            alpha_1[i] = e_alpha[i] - alpha_0[i];
        }
        std::vector<std::vector<long long>> r0(q, std::vector<long long>(n)), r1(q, std::vector<long long>(n));
        for (int i = 0; i < q; i++) {
            for (int j = 0; j < n; j++) {
                r0[i][j] = randInt();
                r1[i][j] = ((j == e_alpha[i]) ? 1 : 0) - r0[i][j];
            }
        }

        // DPF key vectors (per-query)
        std::vector<std::vector<long long>> key0(q), key1(q);
        for (int i = 0; i < q; i++) {
            generateDPF(e_alpha[i], 0, n, key0[i], key1[i]);
        }

        boost::asio::io_context io_context;

        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 9002));

        // Accept clients
        tcp::socket socket_p0(io_context);
        acceptor.accept(socket_p0);

        tcp::socket socket_p1(io_context);
        acceptor.accept(socket_p1);

        // Launch all coroutines in parallel
        run_in_parallel(io_context,
            [&]() -> boost::asio::awaitable<void> {
                co_await handle_client(std::move(socket_p0), "P0", x0_k, y0_k, gamma0_k, x0_n, y0_n, gamma0_n,
                                       scalar_x0, scalar_y0, scalar_gamma0, r0, alpha_0, q, key0);
            },
            [&]() -> boost::asio::awaitable<void> {
                co_await handle_client(std::move(socket_p1), "P1", x1_k, y1_k, gamma1_k, x1_n, y1_n, gamma1_n,
                                       scalar_x1, scalar_y1, scalar_gamma1, r1, alpha_1, q, key1);
            }
        );

        io_context.run();

    } catch (std::exception& e) {
        std::cerr << "Exception in P2: " << e.what() << "\n";
    }
}
