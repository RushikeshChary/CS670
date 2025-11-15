#include <fstream>   // for std::ifstream
#include <iostream>  // for std::cout, std::endl
#include <vector>    // for std::vector
#include <string>    // for std::string
#include <cstdint>   // for uint32_t
#include <stdexcept> // for std::runtime_error
#include "common.hpp"
#include "shares.hpp"

#if !defined(ROLE_p0) && !defined(ROLE_p1)
#error "ROLE must be defined as ROLE_p0 or ROLE_p1"
#endif


// ----------------------- Helper coroutines -----------------------
awaitable<void> MPC_DOTPRODUCT(tcp::socket &peer_sock, const std::vector<int> &x, const std::vector<int> &y, int gamma, const std::vector<int> &u, const std::vector<int> &v, int &z) {
    std::vector<int> x_dash = vec_add(u, x);
    std::vector<int> y_dash = vec_add(v, y);
    co_await send_vector1d(peer_sock, x_dash);
    co_await recv_vector1d(peer_sock, x_dash);
    co_await send_vector1d(peer_sock, y_dash);
    co_await recv_vector1d(peer_sock, y_dash);

    // Local computation:
    z = dot_prod(u, (vec_add(y, y_dash))) - dot_prod(v, x_dash) + gamma;
    co_return;
}

awaitable<void> MPC_SCALAR_PRODUCT(tcp::socket &peer_sock, const std::vector<int> &x, const std::vector<int> &y, const std::vector<int> &gamma, const std::vector<int> &u, int delta, std::vector<int> &z) {
    int k = static_cast<int>(x.size());
    std::vector<int> x_dash(k), y_dash(k);
    for (int i = 0; i < k; i++) {
        x_dash[i] = x[i] + u[i];
        y_dash[i] = y[i] + delta;
    }
    co_await send_vector1d(peer_sock, x_dash);
    // std::cout<<"x_dash sent to peer MPC_SCALAR_PRODUCT\n";
    co_await recv_vector1d(peer_sock, x_dash);
    // std::cout<<"x_dash recieved from peer MPC_SCALAR_PRODUCT\n";
    co_await send_vector1d(peer_sock, y_dash);
    // std::cout<<"y_dash sent to peer MPC_SCALAR_PRODUCT\n";
    co_await recv_vector1d(peer_sock, y_dash);
    // std::cout<<"y_dash recieved from peer MPC_SCALAR_PRODUCT\n";

    z.assign(k, 0);
    // Local computation:
    for (int i = 0; i < k; i++) {
        z[i] = u[i]*(delta + y_dash[i]) - (x_dash[i] * y[i]) + gamma[i];
        // std::cout<<z[i]<<" ";
    }
    // std::cout<<"MPC_SCALAR_PRODUCT local computation done\n";
    co_return;
}

std::vector<int> colwise_dot(const std::vector<std::vector<int>> &A,
                             const std::vector<std::vector<int>> &B) {
    if (A.size() != B.size() || A.empty() || A[0].size() != B[0].size()) {
        throw std::invalid_argument("Matrix dimensions must match for column-wise dot product.");
    }

    int n_rows = static_cast<int>(A.size());
    int n_cols = static_cast<int>(A[0].size());

    std::vector<int> out(n_cols, 0);

    for (int col = 0; col < n_cols; ++col) {
        int sum = 0;
        for (int row = 0; row < n_rows; ++row) {
            sum += A[row][col] * B[row][col];
        }
        out[col] = sum;
    }

    return out;
}

void log_matrix(std::ofstream &ofs, const std::string &name, const std::vector<std::vector<int>> &mat) {
    ofs << name << " (" << mat.size() << "x" << (mat.empty() ? 0 : mat[0].size()) << "):\n";
    for(const auto &row : mat) {
        for(auto val : row) ofs << val << " ";
        ofs << "\n";
    }
    ofs << std::flush;
}

void log_vector(std::ofstream &ofs, const std::string &name, const std::vector<int> &vec) {
    ofs << name << ": ";
    for(auto val : vec) ofs << val << " ";
    ofs << "\n";
    ofs << std::flush;
}

// ----------------------- Setup connections -----------------------

// Setup connection to P2 (P0/P1 act as clients, P2 acts as server)
awaitable<tcp::socket> setup_server_connection(boost::asio::io_context& io_context, tcp::resolver& resolver) {
    tcp::socket sock(io_context);

    // Connect to P2
    auto endpoints_p2 = resolver.resolve("p2", "9002");
    co_await boost::asio::async_connect(sock, endpoints_p2, use_awaitable);

    co_return sock;
}

// Setup peer connection between P0 and P1
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

    tcp::socket server_sock = co_await setup_server_connection(io_context, resolver);
    tcp::socket peer_sock = co_await setup_peer_connection(io_context, resolver);
    std::cout<<"All connection set... Proceed!\n";

    std::string q_file;
    std::string output_file;

#ifdef ROLE_p0
    q_file = "f1.txt";
    output_file = "o1.txt";
#else
    q_file = "f2.txt";
    output_file = "o2.txt";
#endif
    // std::cout << "Using query file: " << q_file << std::endl;
    //Read input data and queries from file
    std::ifstream ifs(q_file);
    std::ofstream ofs(output_file);
    if (!ifs) {
        std::cerr << "Error opening file for reading: " << q_file << std::endl;
        co_return;
    }
    if (!ofs) {
        std::cerr << "Error opening file for writing: " << output_file << std::endl;
        co_return;
    }
    int m,n,k,q;
    ifs >> m >> n >> k >> q;
    for(int i = 0;i<q;i++){
        // extract the quiery
        int user_index, item_index_share;
        ifs >> user_index >> item_index_share;
        // Initialize shares
        Share share(n,m,k);

        // write them to some output file (just for the sake of sanity check).
    // Log role and received values
#ifdef ROLE_p0
    ofs << "=== Role: P0 | Query " << i << " ===\n";
#else
    ofs << "=== Role: P1 | Query " << i << " ===\n";
#endif
        log_matrix(ofs, "User feature matrix u", share.u);
        log_matrix(ofs, "Item feature matrix v", share.v);
        log_matrix(ofs, "Random matrix r", share.r);
        // Here the protocol begins.
        // Step 1: Receive e_alpha share and alpha from server.
        // Now, let us start getting variables from server for performing rotation trick.
        co_await recv_vector1d(server_sock, share.e_alpha);
        // std::cout<<"e_alpha recieved from p2\n";
        co_await recv_int32(server_sock, share.alpha);
        // std::cout<<"alpha recieved from p2\n";
        log_vector(ofs, "e_alpha", share.e_alpha);
        ofs << "alpha: " << share.alpha << "\n";



        int local_diff = item_index_share - share.alpha;
        co_await send_int32(peer_sock, local_diff);
        // std::cout<<"local_diff sent to peer\n";
        co_await recv_int32(peer_sock, local_diff);
        // std::cout<<"local_diff recieved from peer\n";
        int shift = local_diff + (item_index_share - share.alpha); // since both parties have same local_diff
        std::vector<int> e_j = rotate_cyclic(share.e_alpha, shift);
        ofs << "shift: "<< shift << "\n";
        log_vector(ofs, "e_j", e_j);

        // Step 2: Now, we have e_j share. Next, we need to share masked V database.
        // start with sharing blinded V database.
        co_await send_vector2d(peer_sock, share.v_dash);
        // std::cout<<"v_dash sent to peer\n";
        co_await recv_vector2d(peer_sock, share.v_dash);
        // std::cout<<"v_dash recieved from peer\n";

        log_matrix(ofs, "v_dash (after exchange)", share.v_dash);
        // Now compute the masked V database.
        for(int i = 0;i<n;i++){
            for(int j = 0;j<k;j++){
                share.v_masked[i][j] = share.v_dash[i][j] + share.r[i][j] + share.v[i][j];
            }
        }
        
        log_matrix(ofs, "v_masked", share.v_masked);

        // Now, we have to perform dot product <D + r0 + r1, f0> or <D + r0 + r1, f1>
        // getting v_j shares.
        std::vector<int> v_j_share = compute_v_share(e_j, share.v_masked);
        log_vector(ofs, "v_j_share (masked)", v_j_share);
        // Task is to unmask these shares now....
        // Here D is matrix.. So, let us extrapolate the e_j shares (f0, f1) to matrices and perform a column vise dot product.
        std::vector<std::vector<int>> e_j_matrix(n, std::vector<int>(k,0));
        for(int i = 0;i<n;i++) e_j_matrix[i] = std::vector<int>(k, e_j[i]);

        log_matrix(ofs, "e_j_matrix", e_j_matrix);
        // Du-Atallah
        // Get some variables from p2.
        co_await recv_vector2d(server_sock, share.x_n);
        // std::cout<<"x_n recieved from p2\n";
        co_await recv_vector2d(server_sock, share.y_n);
        // std::cout<<"y_n recieved from p2\n";
        co_await recv_vector1d(server_sock, share.gamma_n);
        // std::cout<<"gamma_n recieved from p2\n";

        log_matrix(ofs, "x_n", share.x_n);
        log_matrix(ofs, "y_n", share.y_n);
        log_vector(ofs, "gamma_n", share.gamma_n);

        // Extra vectors to communicate:
        std::vector<std::vector<int>> x_dash(n, std::vector<int>(k));
        std::vector<std::vector<int>> y_dash(n, std::vector<int>(k));
        // Now, mask your share
        for(int i = 0;i<n;i++){
            for(int j = 0;j<k;j++){
                x_dash[i][j] = share.x_n[i][j] + e_j_matrix[i][j];
                y_dash[i][j] = share.y_n[i][j] + share.r[i][j];
            }
        }

        // communicate to peers
        co_await send_vector2d(peer_sock, x_dash);
        // std::cout<<"x_dash sent to peer\n";
        co_await recv_vector2d(peer_sock, x_dash);
        // std::cout<<"x_dash recieved from peer\n";
        co_await send_vector2d(peer_sock, y_dash);
        // std::cout<<"y_dash sent to peer\n";
        co_await recv_vector2d(peer_sock, y_dash);
        // std::cout<<"y_dash recieved from peer\n";


        log_matrix(ofs, "x_dash (after exchange)", x_dash);
        log_matrix(ofs, "y_dash (after exchange)", y_dash);
        // Local computation to get final dotproduct.
        for(int i = 0; i<n;i++){
            for(int j = 0;j<k;j++){
                y_dash[i][j] += share.r[i][j];
            }
        }
        // Few more temparary variables.
        std::vector<int> r_share = colwise_dot(e_j_matrix, y_dash);
        std::vector<int> second_term = colwise_dot(share.y_n, x_dash);
        for(int i = 0;i<k;i++) r_share[i] = r_share[i] - second_term[i] + share.gamma_n[i];
        log_vector(ofs, "r_share", r_share);
        // Now, r_share is the share of dot product. Now remove mask.
        for(int i = 0;i<k;i++) v_j_share[i] = v_j_share[i] - r_share[i];
        log_vector(ofs, "v_j_share (unmasked)", v_j_share);
        // Now, v_j_share is the unmasked share of dot product.
        // print this final unmasked v_j_share in terminal.
        // std::cout<<"Final v_j_share (unmasked) computed: ";
        // for(auto val : v_j_share) std::cout<<val<<" ";
        // std::cout<<"\n\n";


        // Now, let us proceed with computing delta shares.
        co_await recv_vector1d(server_sock, share.x_k);
        co_await recv_vector1d(server_sock, share.y_k);
        co_await recv_int32(server_sock, share.gamma_k);
        // std::cout<<"x_k, y_k, gamma_k recieved from p2\n";
        log_vector(ofs, "x_k", share.x_k);
        log_vector(ofs, "y_k", share.y_k);
        ofs << "gamma_k: " << share.gamma_k << "\n";
        int inn_product;
        co_await MPC_DOTPRODUCT(peer_sock, share.x_k, share.y_k, share.gamma_k, share.u[user_index], v_j_share, inn_product);
        // Now lets get shares of delta:
        int delta;
#ifdef ROLE_p0
        delta = 1 - inn_product;
#else
        delta = - inn_product;
#endif
        ofs << "Inner product share: " << inn_product << "\n";
        // Now, let us go ahead with scalar dot product.
        co_await recv_vector1d(server_sock, share.scaler_x);
        co_await recv_vector1d(server_sock, share.scaler_y);
        co_await recv_vector1d(server_sock, share.scaler_gamma);
        // std::cout<<"Sizes: scaler_x, scaler_y, scaler_gamma recieved from p2 are: "<<share.scaler_x.size()<<" "<<share.scaler_y.size()<<" "<<share.scaler_gamma.size()<<"\n";
        log_vector(ofs, "scaler_x", share.scaler_x);
        log_vector(ofs, "scaler_y", share.scaler_y);
        log_vector(ofs, "scaler_gamma", share.scaler_gamma);
        std::vector<int> result(k);
        co_await MPC_SCALAR_PRODUCT(peer_sock, share.scaler_x, share.scaler_y, share.scaler_gamma, v_j_share, delta, result);

        // Do the final update to user database now.
        share.u[user_index] = vec_add(share.u[user_index], result);
        log_matrix(ofs, "Final updated user feature vector", share.u);
        std::cout<<"User database updated.\n";
    }

    co_return;
}

int main() {
    std::cout.setf(std::ios::unitbuf); // auto-flush cout for Docker logs
    boost::asio::io_context io_context(1);
    co_spawn(io_context, run(io_context), boost::asio::detached);
    io_context.run();
    return 0;
}
