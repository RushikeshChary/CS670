#include "common.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <random>
#include <vector>
#include <cstdint>
#include <algorithm> // for std::rotate
#include <fstream>

using boost::asio::ip::tcp;

// Global variables
// For all queries
std::vector<std::pair<uint32_t,uint32_t>> ks;  // additive shares of k per query
std::vector<std::pair<std::vector<uint32_t>, std::vector<uint32_t>>> r_shares; // additive shares of e_k
uint32_t n = 0,m = 0,k_unused = 0,Q = 0;

// Helper functions:
// Generate additive shares of a value v
std::pair<uint32_t, uint32_t> additive_shares(uint32_t v)
{
    uint32_t share1 = random_uint32();
    uint32_t share2 = v - share1;
    // std::cout<<"sum of shares of "<<v<<" = "<<share1+share2<<"\n";
    return {share1, share2};
}

// Generate additive shares of a standard basis vector e_k
std::pair<std::vector<uint32_t>, std::vector<uint32_t>>
vector_shares(int n, int k)
{
    std::vector<uint32_t> r0(n), r1(n);
    for (int i = 0; i < n; i++)
    {
        if (i == k)
        {
            auto shares = additive_shares(1);
            r0[i] = shares.first;
            r1[i] = shares.second;
        }
        else
        {
            auto shares = additive_shares(0);
            r0[i] = shares.first;
            r1[i] = shares.second;
        }
    }
    return {r0, r1};
}


// Read only header (m n k Q) from a query file
bool read_header_from_file(const std::string &filename, uint32_t &m, uint32_t &n, uint32_t &k, uint32_t &Q)
{
    std::ifstream in(filename);
    if (!in) return false;
    if (!(in >> m >> n >> k >> Q)) return false;
    return true;
}

// Random number generator

// Send a number to a client
boost::asio::awaitable<void> handle_client(tcp::socket socket, const std::string &name)
{
    // uint32_t rnd = random_uint32();
    // std::cout << "P2 sending to " << name << ": " << rnd << "\n";
    // co_await boost::asio::async_write(socket, boost::asio::buffer(&rnd, sizeof(rnd)), boost::asio::use_awaitable);

    // Instead of sending a random value, start sending useful info.
    try {
        for (int q = 0; q < Q; ++q) {

            // Send vector-share
            const std::vector<uint32_t>& vec_share_to_send = (name == "P0" ? r_shares[q].first : r_shares[q].second);
            co_await boost::asio::async_write(
                socket,
                boost::asio::buffer(vec_share_to_send.data(), n * sizeof(uint32_t)),
                boost::asio::use_awaitable
            );
            // Send k-share
            uint32_t k_share_to_send = (name == "P0" ? ks[q].first : ks[q].second);
            co_await boost::asio::async_write(
                socket,
                boost::asio::buffer(&k_share_to_send, sizeof(k_share_to_send)),
                boost::asio::use_awaitable
            );

            std::cout << "P2 -> " << name
                      << " : sent query " << q
                      << " (k_share=" << k_share_to_send
                      << ", vector-share sent)\n";
        }
    } catch (const std::exception &ex) {
        std::cerr << "Exception in handle_client for " << name << ": " << ex.what() << "\n";
    }
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
        bool ok = read_header_from_file("f1.txt", m, n, k_unused, Q);
        if (!ok) {
            ok = read_header_from_file("f2.txt", m, n, k_unused, Q);
        }
        if (!ok) {
            std::cout << "Warning: no f1.txt or f2.txt found or readable — using defaults n=" << n << " Q=" << Q << "\n";
        } else {
            std::cout << "P2 read header: m=" << m << " n=" << n << " k=" << k_unused << " Q=" << Q << "\n";
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

        // Step 0: generate all shares for all queries
        ks.resize(Q);
        r_shares.resize(Q);

        for (int q = 0; q < Q; ++q) {
            uint32_t k = random_uint32() % n;
            std::cout<<"chosen k = "<<k<<" \n";
            // generate additive shares of k
            ks[q] = additive_shares(k);
            // std::cout<<"first: "<<ks[q].first<<" second: "<<ks[q].second<<"thier sum = "<<ks[q].first + ks[q].second<<"\n";

            // generate additive shares of e_k
            r_shares[q] = vector_shares(n, k);
        }

        // Launch all coroutines in parallel
        run_in_parallel(io_context, [&]() -> boost::asio::awaitable<void>
                        { co_await handle_client(std::move(socket_p0), "P0"); }, [&]() -> boost::asio::awaitable<void>
                        { co_await handle_client(std::move(socket_p1), "P1"); });

        io_context.run();

        //
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception in P2: " << e.what() << "\n";
    }
}
