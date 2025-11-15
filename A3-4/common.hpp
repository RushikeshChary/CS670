#pragma once
#include <utility>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <iostream>
#include <random>
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
namespace this_coro = boost::asio::this_coro;

// inline uint32_t random_uint32() {
//     static std::random_device rd;
//     static std::mt19937 gen(rd());
//     static std::uniform_int_distribution<uint32_t> dis;
//     return dis(gen);
// }

// // Blind by XOR mask
// inline uint32_t blind_value(uint32_t v) {
//     return v ^ 0xDEADBEEF;
// }

// ---------------------- Random utilities ----------------------

// Random int in [lo, hi]
inline int32_t rand_int(int32_t lo = 0, int32_t hi = 10) {
    if (lo > hi) std::swap(lo, hi);
    static std::random_device rd;
    static std::mt19937 mt(rd());
    std::uniform_int_distribution<int32_t> d(lo, hi);
    return d(mt);
}

// Fill a 2D int32_t matrix with random small ints (for testing)
inline void fill_random(std::vector<std::vector<int32_t>>& mat, int32_t lo = 0, int32_t hi = 5) {
    for (auto &row : mat) {
        for (auto &x : row) x = rand_int(lo, hi);
    }
}

// ---------------------- Linear algebra helpers ----------------------

// dot product (returns int64_t to reduce overflow chance)
inline int64_t dot_prod(const std::vector<int32_t>& a, const std::vector<int32_t>& b) {
    if (a.size() != b.size()) throw std::invalid_argument("dot_prod: size mismatch");
    int64_t s = 0;
    for (size_t i=0;i<a.size();++i) s += static_cast<int64_t>(a[i]) * b[i];
    return s;
}

// elementwise add two vectors -> new vector
inline std::vector<int32_t> vec_add(const std::vector<int32_t>& a, const std::vector<int32_t>& b) {
    if (a.size() != b.size()) throw std::invalid_argument("vec_add: size mismatch");
    std::vector<int32_t> out(a.size());
    for (size_t i=0;i<a.size();++i) out[i] = a[i] + b[i];
    return out;
}

inline std::vector<int32_t> vec_sub(const std::vector<int32_t>& a, const std::vector<int32_t>& b) {
    if (a.size() != b.size()) throw std::invalid_argument("vec_add: size mismatch");
    std::vector<int32_t> out(a.size());
    for (size_t i=0;i<a.size();++i) out[i] = a[i] - b[i];
    return out;
}

// rotate right for positive shift, left for negative shift
inline std::vector<int32_t> rotate_cyclic(const std::vector<int32_t>& v, int shift) {
    int n = static_cast<int>(v.size());
    if (n == 0) return {};
    // normalize shift to [0, n-1]
    shift = ((shift % n) + n) % n;
    std::vector<int32_t> out(n);
    for (int i = 0; i < n; ++i) out[(i + shift) % n] = v[i];
    return out;
}

// get column from matrix (copy)
inline std::vector<int32_t> column_of(const std::vector<std::vector<int32_t>>& mat, size_t col) {
    if (mat.empty()) return {};
    if (col >= mat[0].size()) throw std::out_of_range("column_of: col out of range");
    std::vector<int32_t> c(mat.size());
    for (size_t i=0;i<mat.size();++i) c[i] = mat[i][col];
    return c;
}

// get row from matrix (copy)
inline std::vector<int32_t> row_of(const std::vector<std::vector<int32_t>>& mat, size_t row) {
    if (row >= mat.size()) throw std::out_of_range("row_of: row out of range");
    return mat[row];
}

// produce additive shares of integer v (two shares s0,s1 with s0+s1 = v)
inline std::pair<int32_t,int32_t> make_additive_shares_int(int32_t v) {
    int32_t s0 = static_cast<int32_t>(rand_int());
    int32_t s1 = v - s0;
    return {s0, s1};
}

// produce additive shares of standard basis vector e_k of length n
inline std::pair<std::vector<int32_t>, std::vector<int32_t>> make_basis_vector_shares(size_t n, size_t k) {
    std::vector<int32_t> a(n), b(n);
    for (size_t i=0;i<n;++i) {
        if (i == k) {
            auto p = make_additive_shares_int(1);
            a[i] = p.first; b[i] = p.second;
        } else {
            auto p = make_additive_shares_int(0);
            a[i] = p.first; b[i] = p.second;
        }
    }
    return {a,b};
}

// compute v_masked dot e_i-like share retrieval (sum over rows)
inline std::vector<int32_t> compute_v_share(const std::vector<int32_t>& e_i, const std::vector<std::vector<int32_t>>& V_masked) {
    size_t n = V_masked.size();
    if (e_i.size() != n) throw std::invalid_argument("vector_lookup_by_indicator: size mismatch");
    if (n == 0) return {};
    size_t k = V_masked[0].size();
    std::vector<int32_t> out(k, 0);
    for (size_t i=0;i<n;++i) {
        for (size_t j=0;j<k;++j) out[j] += e_i[i] * V_masked[i][j];
    }
    return out;
}

// ---------------------- Networking helpers (send/recv) ----------------------

// Send a single 32-bit integer
awaitable<void> send_int32(tcp::socket& sock, int32_t v) {
    co_await boost::asio::async_write(sock, boost::asio::buffer(&v, sizeof(v)), use_awaitable);
    co_return;
}

// Receive a single 32-bit integer
awaitable<void> recv_int32(tcp::socket& sock, int32_t& out) {
    co_await boost::asio::async_read(sock, boost::asio::buffer(&out, sizeof(out)), use_awaitable);
    co_return;
}

// Send a 1D vector<int32_t> as raw bytes (caller must ensure size is known by receiver)
awaitable<void> send_vector1d(tcp::socket& sock, const std::vector<int32_t>& v) {
    if (!v.empty()) {
        co_await boost::asio::async_write(sock, boost::asio::buffer(v.data(), v.size() * sizeof(int32_t)), use_awaitable);
    }
    co_return;
}

// Receive a 1D vector<int32_t> into pre-sized buffer
awaitable<void> recv_vector1d(tcp::socket& sock, std::vector<int32_t>& v) {
    if (!v.empty()) {
        co_await boost::asio::async_read(sock, boost::asio::buffer(v.data(), v.size() * sizeof(int32_t)), use_awaitable);
    }
    co_return;
}

// Send 2D matrix row-by-row (assumes rows are contiguous)
awaitable<void> send_vector2d(tcp::socket& sock, const std::vector<std::vector<int32_t>>& mat) {
    for (const auto &row : mat) {
        if (!row.empty()) {
            co_await boost::asio::async_write(sock, boost::asio::buffer(row.data(), row.size() * sizeof(int32_t)), use_awaitable);
        }
    }
    co_return;
}

// Receive 2D matrix row-by-row into pre-sized matrix
awaitable<void> recv_vector2d(tcp::socket& sock, std::vector<std::vector<int32_t>>& mat) {
    for (auto &row : mat) {
        if (!row.empty()) {
            co_await boost::asio::async_read(sock, boost::asio::buffer(row.data(), row.size() * sizeof(int32_t)), use_awaitable);
        }
    }
    co_return;
}

// ---------------------- Debug helpers ----------------------

inline void print_vector(const std::vector<int32_t>& v, const std::string& name="") {
    if (!name.empty()) std::cout << name << " : ";
    for (auto x : v) std::cout << x << " ";
    std::cout << std::endl;
}

inline void print_matrix(const std::vector<std::vector<int32_t>>& mat, const std::string& name="") {
    if (!name.empty()) std::cout << name << " (" << mat.size() << "x" << (mat.empty()?0:mat[0].size()) << ")\n";
    for (const auto& row : mat) {
        for (auto x : row) std::cout << x << " ";
        std::cout << "\n";
    }
}



