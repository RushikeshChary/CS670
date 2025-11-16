// pB.cpp (rewritten to use the renamed routines in key.h / shares.h)
//
// Logic is unchanged from your previous version — names updated to match the
// provided key.h and shares.h (DPFKey/build_key_from_vector, MPCShare, vec_dot,
// vec_add, circular_rotate, compute_v_share, and async_* I/O helpers).

#include "shares.h"

#if !defined(ROLE_p0) && !defined(ROLE_p1)
#error "ROLE must be defined as ROLE_p0 or ROLE_p1"
#endif

// ----------------------- Helper coroutines -----------------------
awaitable<void> MPC_DOTPRODUCT(std::vector<long long> &x, std::vector<long long> &y, long long gamma,
                              tcp::socket &peer_sock,
                              std::vector<long long> &u, std::vector<long long> &v, long long &ans) {
    // x_tilda = u + x, y_tilda = v + y
    std::vector<long long> x_tilda = vec_add(u, x);
    std::vector<long long> y_tilda = vec_add(v, y);

    co_await async_send_vector1d(peer_sock, x_tilda);
    co_await async_recv_vector1d(peer_sock, x_tilda);
    co_await async_send_vector1d(peer_sock, y_tilda);
    co_await async_recv_vector1d(peer_sock, y_tilda);

    ans = vec_dot(u, vec_add(v, y_tilda)) - vec_dot(y, x_tilda) + gamma;
    co_return;
}

awaitable<void> MPC_SCALAR_PRODUCT(MPCShare &S, tcp::socket &peer_sock,
                                  const std::vector<long long> &v, long long scalar,
                                  std::vector<long long> &result) {
    // x_tilda = v + scalerX, y_tilda = scalar + scalerY
    std::vector<long long> x_tilda = vec_add(v, std::vector<long long>(v.size(), S.scalarX));
    long long y_tilda = scalar + S.scalarY;

    co_await async_send_vector1d(peer_sock, x_tilda);
    co_await async_recv_vector1d(peer_sock, x_tilda);
    co_await async_send_scalar(peer_sock, y_tilda);
    co_await async_recv_scalar(peer_sock, y_tilda);

    for (size_t i = 0; i < v.size(); ++i) {
        result[i] = v[i] * (scalar + y_tilda) - S.scalarY * (x_tilda[i]) + S.scalarGamma;
    }
    co_return;
}

std::vector<long long> columnwise_dot_product(std::vector<std::vector<long long>> &A,
                                              std::vector<std::vector<long long>> &B) {
    int rows = static_cast<int>(A.size());
    int cols = static_cast<int>(A[0].size());
    std::vector<long long> result(cols, 0);
    for (int j = 0; j < cols; ++j) {
        for (int i = 0; i < rows; ++i) {
            result[j] += A[i][j] * B[i][j];
        }
    }
    return result;
}

// ----------------------- Setup connections -----------------------

// connect to P2 (P0/P1 act as clients)
awaitable<tcp::socket> setup_server_connection(boost::asio::io_context& io_context, tcp::resolver& resolver) {
    tcp::socket sock(io_context);
    auto endpoints_p2 = resolver.resolve("p2", "9002");
    co_await boost::asio::async_connect(sock, endpoints_p2, use_awaitable);
    co_return sock;
}

// peer connection between P0 and P1
awaitable<tcp::socket> setup_peer_connection(boost::asio::io_context& io_context, tcp::resolver& resolver) {
    tcp::socket sock(io_context);
#ifdef ROLE_p0
    auto endpoints_p1 = resolver.resolve("p1", "9001");
    co_await boost::asio::async_connect(sock, endpoints_p1, use_awaitable);
#else
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 9001));
    sock = co_await acceptor.async_accept(use_awaitable);
#endif
    co_return sock;
}

// ----------------------- Main protocol -----------------------
awaitable<void> run(boost::asio::io_context& io_context) {
    tcp::resolver resolver(io_context);

    // Step 1: connect to P2
    tcp::socket server_sock = co_await setup_server_connection(io_context, resolver);

    // Step 2: connect to peer (P0 <-> P1)
    tcp::socket peer_sock = co_await setup_peer_connection(io_context, resolver);

    std::cout << "All connected!" << std::endl;

    std::string query_file, output_file;
#ifdef ROLE_p0
    query_file = "/app/files/q0.txt";
    output_file = "/app/files/output0.txt";
#else
    query_file = "/app/files/q1.txt";
    output_file = "/app/files/output1.txt";
#endif

    std::ifstream ifs(query_file);
    std::ofstream ofs(output_file, std::ios::out);
    if (!ifs) {
        std::cerr << "Error opening file for reading: " << query_file << std::endl;
        co_return;
    }
    if (!ofs) {
        std::cerr << "Error opening file for writing: " << output_file << std::endl;
        co_return;
    }

    int n, m, k, q;
    ifs >> n >> m >> k >> q;

    MPCShare S(n, m, k); // initialize shares and ephemeral values

    // write initial U and V shares to output
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < k; ++j) ofs << S.U[i][j] << " ";
        ofs << std::endl;
    }
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < k; ++j) ofs << S.V[i][j] << " ";
        ofs << std::endl;
    }
    ofs.flush();

    // exchange Vt for ORAM read between peers
    co_await async_send_matrix(peer_sock, S.Vt);
    co_await async_recv_matrix(peer_sock, S.Vt);

    // prepare Vmasked = V + R + Vt
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < k; ++j) S.Vmasked[i][j] = S.V[i][j] + S.R[i][j] + S.Vt[i][j];
    }

    long long party;
    co_await async_recv_scalar(server_sock, party); // receive party id
    long long size;
    co_await async_recv_scalar(server_sock, size);
    S.key_vec.resize(size);

    for (int qi = 0; qi < q; ++qi) {
        co_await async_recv_vector1d(server_sock, S.key_vec);
        build_key_from_vector(S.key_vec, S.dpf_key);

        co_await async_recv_vector1d(server_sock, S.eAlpha); // shared randomness for rotation trick
        co_await async_recv_scalar(server_sock, S.alpha);

        co_await async_recv_vector1d(server_sock, S.xK); // k-dimension du-atallah
        co_await async_recv_vector1d(server_sock, S.yK);
        co_await async_recv_scalar(server_sock, S.gammaK);

        co_await async_recv_vector1d(server_sock, S.xN); // n-dimension du-atallah
        co_await async_recv_vector1d(server_sock, S.yN);
        co_await async_recv_scalar(server_sock, S.gammaN);

        co_await async_recv_scalar(server_sock, S.scalarX); // scalar du-atallah
        co_await async_recv_scalar(server_sock, S.scalarY);
        co_await async_recv_scalar(server_sock, S.scalarGamma);

        int user_id, item_id;
        ifs >> user_id >> item_id;

        long long i_minus_alpha = item_id - S.alpha;
        co_await async_send_scalar(peer_sock, i_minus_alpha);
        co_await async_recv_scalar(peer_sock, i_minus_alpha);

        std::vector<long long> e_i = circular_rotate(S.eAlpha, i_minus_alpha + item_id - S.alpha);
        std::vector<long long> v_share = compute_v_share(e_i, S.Vmasked);

        // build matrices for mask-removal and du-atallah
        std::vector<std::vector<long long>> e_i_matrix(n, std::vector<long long>(k)),
                                    X(n, std::vector<long long>(k)),
                                    Y(n, std::vector<long long>(k)),
                                    X_tilda(n, std::vector<long long>(k)),
                                    Y_tilda(n, std::vector<long long>(k));
        for (int j = 0; j < n; ++j) e_i_matrix[j] = std::vector<long long>(k, e_i[j]);
        for (int j = 0; j < n; ++j) X[j] = std::vector<long long>(k, S.xN[j]);
        for (int j = 0; j < n; ++j) Y[j] = std::vector<long long>(k, S.yN[j]);

        for (int j = 0; j < n; ++j) {
            for (int l = 0; l < k; ++l) {
                X_tilda[j][l] = e_i_matrix[j][l] + X[j][l];
                Y_tilda[j][l] = S.R[j][l] + Y[j][l];
            }
        }

        // du-atallah sharing among peers
        co_await async_send_matrix(peer_sock, X_tilda);
        co_await async_recv_matrix(peer_sock, X_tilda);
        co_await async_send_matrix(peer_sock, Y_tilda);
        co_await async_recv_matrix(peer_sock, Y_tilda);

        // adjust Y_tilda with local R
        for (int j = 0; j < n; ++j) {
            for (int l = 0; l < k; ++l) {
                Y_tilda[j][l] += S.R[j][l];
            }
        }

        std::vector<long long> r_share = columnwise_dot_product(e_i_matrix, Y_tilda);
        std::vector<long long> temp = columnwise_dot_product(X_tilda, Y);
        for (int j = 0; j < k; ++j) r_share[j] = r_share[j] - temp[j] + S.gammaN;
        for (int j = 0; j < k; ++j) v_share[j] -= r_share[j];

        long long z;
        co_await MPC_DOTPRODUCT(S.xK, S.yK, S.gammaK, peer_sock, S.U[user_id], v_share, z);

        long long delta;
#ifdef ROLE_p0
        delta = 1 - z;
#else
        delta = -z;
#endif

        std::vector<long long> u_delta(k), v_delta(k);
        co_await MPC_SCALAR_PRODUCT(S, peer_sock, S.U[user_id], delta, u_delta);
        co_await MPC_SCALAR_PRODUCT(S, peer_sock, v_share, delta, v_delta);

        for (int j = 0; j < k; ++j) {
            long long FCW = (S.dpf_key.sign ? -1 : 1) * u_delta[j] + S.dpf_key.final_cw;
            co_await async_send_scalar(peer_sock, FCW);
            co_await async_recv_scalar(peer_sock, FCW);
            FCW += ((S.dpf_key.sign ? -1 : 1) * u_delta[j] + S.dpf_key.final_cw);

            std::vector<long long> dpf_eval = evalDPF(S.dpf_key, n, party, FCW);

            long long i_minus_alpha2 = item_id - S.alpha;
            co_await async_send_scalar(peer_sock, i_minus_alpha2);
            co_await async_recv_scalar(peer_sock, i_minus_alpha2);
            dpf_eval = circular_rotate(dpf_eval, i_minus_alpha2 + item_id - S.alpha);

            for (int l = 0; l < n; ++l) {
                S.V[l][j] += dpf_eval[l];
            }
        }

        // refresh R and Vt / Vmasked
        fill_random_matrix(S.R);
        for (int ii = 0; ii < n; ++ii) {
            for (int jj = 0; jj < k; ++jj) S.Vt[ii][jj] = S.V[ii][jj] + S.R[ii][jj];
        }
        co_await async_send_matrix(peer_sock, S.Vt);
        co_await async_recv_matrix(peer_sock, S.Vt);

        for (int ii = 0; ii < n; ++ii) {
            for (int jj = 0; jj < k; ++jj) S.Vmasked[ii][jj] = S.V[ii][jj] + S.R[ii][jj] + S.Vt[ii][jj];
        }

        for (int j = 0; j < n; ++j) {
            for (int l = 0; l < k; ++l) ofs << S.V[j][l] << " ";
            ofs << std::endl;
        }
        ofs.flush();
    }

    ifs.close();
    ofs.close();
    co_return;
}

int main() {
    std::cout.setf(std::ios::unitbuf); // auto-flush cout for Docker logs
    boost::asio::io_context io_context(1);
    co_spawn(io_context, run(io_context), boost::asio::detached);
    io_context.run();
    return 0;
}
