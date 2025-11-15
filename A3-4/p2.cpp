#include "common.hpp"
// #include "shares.hpp"

using boost::asio::ip::tcp;

int m,n,k,Q;
// Send a number to a client
boost::asio::awaitable<void> handle_client(tcp::socket socket, const std::string &name, const std::vector<std::vector<int>> &e_alpha, std::vector<int> &alpha, std::vector<std::vector<std::vector<int>>> &matrix_xn, std::vector<std::vector<std::vector<int>>> &matrix_yn, std::vector<std::vector<int>> & gamma_shares, std::vector<std::vector<int>> &mpc_vec_xk, std::vector<std::vector<int>> &mpc_vec_yk, std::vector<int> &mpc_gamma_shares, std::vector<std::vector<int>> &mpc_scaler_xk_shares, std::vector<std::vector<int>> &mpc_scaler_yk_shares, std::vector<std::vector<int>> &mpc_scaler_gamma_shares)
{
    try {
        for (int q = 0; q < Q; ++q) {
            
            co_await send_vector1d(socket, e_alpha[q]); // send e_alpha share (1d vector of length n)
            // std::cout<<"Sent e_alpha share to "<<name<<"\n"; 
            co_await send_int32(socket, alpha[q]); // send alpha share (single int)
            // std::cout<<"Sent alpha share to "<<name<<"\n";

            co_await send_vector2d(socket, matrix_xn[q]); // send x_n share (2d vector of size n * k)
            // std::cout<<"Sent x_n share to "<<name<<"\n";
            co_await send_vector2d(socket, matrix_yn[q]); // send y_n share (2d vector of size n * k)
            // std::cout<<"Sent y_n share to "<<name<<"\n";
            co_await send_vector1d(socket, gamma_shares[q]); // send gamma share (1d vector of length k)
            // std::cout<<"Sent gamma share to "<<name<<"\n";

            co_await send_vector1d(socket, mpc_vec_xk[q]); // send x_k share (1d vector of length k)
            // std::cout<<"Sent x_k share to "<<name<<"\n";
            co_await send_vector1d(socket, mpc_vec_yk[q]); // send y_k share (1d vector of length k)
            // std::cout<<"Sent y_k share to "<<name<<"\n";
            co_await send_int32(socket, mpc_gamma_shares[q]); // send gamma_k share (single int)
            // std::cout<<"Sent gamma_k share to "<<name<<"\n";

            co_await send_vector1d(socket, mpc_scaler_xk_shares[q]); // send scaler_xk share (1d vector of length k)
            // std::cout<<"Sent scaler_xk share to "<<name<<"\n";
            co_await send_vector1d(socket, mpc_scaler_yk_shares[q]); // send scaler_yk share (1d vector of length k)
            // std::cout<<"Sent scaler_yk share to "<<name<<"\n";
            // std::cout<<"mpc_scaler_gamma_shares[q] size: "<<mpc_scaler_gamma_shares[q].size()<<"\n";
            co_await send_vector1d(socket, mpc_scaler_gamma_shares[q]);
            // std::cout<<"Sent scaler_gamma share to "<<name<<"\n";

        }
    } catch (const std::exception &ex) {
        std::cerr << "Exception in handle_client for " << name << ": " << ex.what() << "\n";
    }
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

// Read only header (m n k Q) from a query file
bool read_header_from_file(const std::string &filename, int &m, int &n, int &k, int &Q)
{
    std::ifstream in(filename);
    if (!in) return false;
    if (!(in >> m >> n >> k >> Q)) return false;
    return true;
}

// Run multiple coroutines in parallel
template <typename... Fs>
void run_in_parallel(boost::asio::io_context &io, Fs &&...funcs)
{
    (boost::asio::co_spawn(io, funcs, boost::asio::detached), ...);
}

int main()
{
    try
    {
        
        // 1) Read header (m,n,k,Q) from f1.txt or f2.txt (P0/P1 files). If not found, fallback to defaults.
        bool ok = read_header_from_file("f1.txt", m, n, k, Q);
        if (!ok) {
            ok = read_header_from_file("f2.txt", m, n, k, Q);
        }
        if (!ok) {
            std::cout << "Warning: no f1.txt or f2.txt found or readable — using defaults n=" << n << " Q=" << Q << "\n";
        } else {
            std::cout << "P2 read header: m=" << m << " n=" << n << " k=" << k << " Q=" << Q << "\n";
        }
        
        // Step 0: generate all shares for all queries
        
        // Storage units for both parties.
        std::vector<int> alpha_shares_p0, alpha_shares_p1; // each is a single int
        std::vector<std::vector<int>> e_alpha_shares_p0, e_alpha_shares_p1; // each is Q vectors of length n
        std::vector<std::vector<std::vector<int>>> matrix_xn_shares_p0, matrix_xn_shares_p1; // each is Q matrices of size n * k (i.e., x_n)
        std::vector<std::vector<std::vector<int>>> matrix_yn_shares_p0, matrix_yn_shares_p1; // each is Q matrices of size n * k (i.e., x_n)
        std::vector<int> gamma_p0, gamma_p1; // each is a vector of length k
        std::vector<std::vector<int>> gamma_shares_p0, gamma_shares_p1;
        std::vector<std::vector<int>> mpc_vec_xk_shares_p0, mpc_vec_xk_shares_p1; // each is a vector of length k
        std::vector<std::vector<int>> mpc_vec_yk_shares_p0, mpc_vec_yk_shares_p1; // each is a vector of length k
        int vec_gamma_k_p0, vec_gamma_k_p1; // each is a single int
        std::vector<int> mpc_gamma_shares_p0, mpc_gamma_shares_p1; // each is a single int
        std::vector<std::vector<int>> mpc_scaler_xk_shares_p0, mpc_scaler_xk_shares_p1; // each is a vector of length k
        std::vector<std::vector<int>> mpc_scaler_yk_shares_p0, mpc_scaler_yk_shares_p1; // each is a vector of length k
        std::vector<std::vector<int>> mpc_scaler_gamma_shares_p0, mpc_scaler_gamma_shares_p1; // each is a single int
        for (int q = 0; q < Q; ++q) {
            // Firstly, let us get alpha shares(int) and e_alpha shares (1d vector).
            int alpha = rand_int(0, n - 1);
            std::cout<<"Query "<<q<<": alpha = "<<alpha<<"\n";
            auto [alpha_p0, alpha_p1] = make_additive_shares_int(alpha);
            alpha_shares_p0.push_back(alpha_p0);
            alpha_shares_p1.push_back(alpha_p1);
            auto [e_alpha_share_p0, e_alpha_share_p1] = make_basis_vector_shares(n, alpha);
            e_alpha_shares_p0.push_back(e_alpha_share_p0);
            e_alpha_shares_p1.push_back(e_alpha_share_p1);

            // Now is the time for creating random 2d vectors and store them.
            std::vector<std::vector<int>> mat_p0(n, std::vector<int>(k));
            std::vector<std::vector<int>> mat_p1(n, std::vector<int>(k));
            // for xn
            fill_random(mat_p0);
            fill_random(mat_p1);
            matrix_xn_shares_p0.push_back(mat_p0);
            matrix_xn_shares_p1.push_back(mat_p1);
            // for yn
            fill_random(mat_p0);
            fill_random(mat_p1);
            matrix_yn_shares_p0.push_back(mat_p0);
            matrix_yn_shares_p1.push_back(mat_p1);

            // Now get gamma --> i.e., colwise dot product of x_n and y_n
            gamma_p0 = colwise_dot(matrix_xn_shares_p0[q], matrix_yn_shares_p1[q]);
            gamma_p1 = colwise_dot(matrix_xn_shares_p1[q], matrix_yn_shares_p0[q]);
            
            // Add a random vector to gamma_p0 and subtract it from gamma_p1
            std::vector<int> gamma_mask(k);
            for(int i = 0;i<k;i++) gamma_mask[i] = rand_int();
            for(int i = 0;i<k;i++) {
                gamma_p0[i] += gamma_mask[i];
                gamma_p1[i] -= gamma_mask[i];
            }
            gamma_shares_p0.push_back(gamma_p0);
            gamma_shares_p1.push_back(gamma_p1);
            // Now let us make vectors for MPC dotprouct.
            std::vector<int> mpc_dot_xk_p0(k), mpc_dot_xk_p1(k);
            std::vector<int> mpc_dot_yk_p0(k), mpc_dot_yk_p1(k);
            for(int i = 0;i<k;i++) {
                mpc_dot_xk_p0[i] = rand_int();
                mpc_dot_xk_p1[i] = rand_int();
                mpc_dot_yk_p0[i] = rand_int();
                mpc_dot_yk_p1[i] = rand_int();
            }
            mpc_vec_xk_shares_p0.push_back(mpc_dot_xk_p0);
            mpc_vec_xk_shares_p1.push_back(mpc_dot_xk_p1);
            mpc_vec_yk_shares_p0.push_back(mpc_dot_yk_p0);
            mpc_vec_yk_shares_p1.push_back(mpc_dot_yk_p1);

            // Now get vec_gamma_k.
            vec_gamma_k_p0 = dot_prod(mpc_vec_xk_shares_p0[q], mpc_vec_yk_shares_p1[q]);
            vec_gamma_k_p1 = dot_prod(mpc_vec_xk_shares_p1[q], mpc_vec_yk_shares_p0[q]);
            int gamma_mask_scalar = rand_int();
            vec_gamma_k_p0 += gamma_mask_scalar;
            vec_gamma_k_p1 -= gamma_mask_scalar;
            mpc_gamma_shares_p0.push_back(vec_gamma_k_p0);
            mpc_gamma_shares_p1.push_back(vec_gamma_k_p1);

            // Now generate shares for mpc scalar multiplication.
            std::vector<int> mpc_scalar_xk_p0(k), mpc_scalar_xk_p1(k);
            std::vector<int> mpc_scalar_yk_p0(k), mpc_scalar_yk_p1(k);
            for(int i = 0;i<k;i++) {
                mpc_scalar_xk_p0[i] = rand_int();
                mpc_scalar_xk_p1[i] = rand_int();
                mpc_scalar_yk_p0[i] = rand_int();
                mpc_scalar_yk_p1[i] = rand_int();
            }
            mpc_scaler_xk_shares_p0.push_back(mpc_scalar_xk_p0);
            mpc_scaler_xk_shares_p1.push_back(mpc_scalar_xk_p1);
            mpc_scaler_yk_shares_p0.push_back(mpc_scalar_yk_p0);
            mpc_scaler_yk_shares_p1.push_back(mpc_scalar_yk_p1);

            // Now get mpc_gamma shares.
            std::vector<int> scaler_gamma_shares_p0, scaler_gamma_shares_p1;
            for(int i = 0;i<k;i++){
                int mpc_scalar_gamma_p0 = mpc_scaler_xk_shares_p0[q][i]*mpc_scaler_yk_shares_p1[q][i];
                int mpc_scalar_gamma_p1 = mpc_scaler_xk_shares_p1[q][i]*mpc_scaler_yk_shares_p0[q][i];
                int gamma_mask_scalar_2 = rand_int();
                mpc_scalar_gamma_p0 += gamma_mask_scalar_2;
                mpc_scalar_gamma_p1 -= gamma_mask_scalar_2;
                
                scaler_gamma_shares_p0.push_back(mpc_scalar_gamma_p0);
                scaler_gamma_shares_p1.push_back(mpc_scalar_gamma_p1);
            }
            mpc_scaler_gamma_shares_p0.push_back(scaler_gamma_shares_p0);
            mpc_scaler_gamma_shares_p1.push_back(scaler_gamma_shares_p1);
        }
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 9002));
        // Accept clients
        // Accept P0.
        tcp::socket socket_p0(io_context);
        acceptor.accept(socket_p0);
        // Accept P1.
        tcp::socket socket_p1(io_context);
        acceptor.accept(socket_p1);
        std::cout<<"Both clients connected to P2. Starting protocol...\n";
        // Launch all coroutines in parallel
        run_in_parallel(io_context, [&]() -> boost::asio::awaitable<void>
                        { co_await handle_client(std::move(socket_p0), "P0", e_alpha_shares_p0, alpha_shares_p0, matrix_xn_shares_p0, matrix_yn_shares_p0, gamma_shares_p0, mpc_vec_xk_shares_p0, mpc_vec_yk_shares_p0, mpc_gamma_shares_p0, mpc_scaler_xk_shares_p0, mpc_scaler_yk_shares_p0, mpc_scaler_gamma_shares_p0);}, [&]() -> boost::asio::awaitable<void>
                        { co_await handle_client(std::move(socket_p1), "P1", e_alpha_shares_p1, alpha_shares_p1, matrix_xn_shares_p1, matrix_yn_shares_p1, gamma_shares_p1, mpc_vec_xk_shares_p1, mpc_vec_yk_shares_p1, mpc_gamma_shares_p1, mpc_scaler_xk_shares_p1, mpc_scaler_yk_shares_p1, mpc_scaler_gamma_shares_p1);});

        io_context.run();

        //
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception in P2: " << e.what() << "\n";
    }
}
