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
#include <numeric>
#include "key.h"

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
namespace this_coro = boost::asio::this_coro;

// small random integer generator
int randInt(int lo = 0, int hi = 5) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(gen);
}

// fill a 2D matrix with small random ints
void fill_random_matrix(std::vector<std::vector<long long>> &mat) {
    for (size_t i = 0; i < mat.size(); ++i) {
        for (size_t j = 0; j < mat[i].size(); ++j) {
            mat[i][j] = randInt();
        }
    }
}

// container for all MPC shares and ephemeral values
class MPCShare {
public:
    int N, M, K; // number of items, users, features
    std::vector<std::vector<long long>> U, V, R, Vt, Vmasked;
    std::vector<long long> xK, xN, yK, yN, eAlpha;
    long long gammaK, gammaN;
    long long scalarX, scalarY, scalarGamma;
    long long alpha;
    DPFKey dpf_key;
    std::vector<long long> key_vec;

    MPCShare(int n, int m, int k) : N(n), M(m), K(k) {
        U.resize(M, std::vector<long long>(K));
        V.resize(N, std::vector<long long>(K));
        R.resize(N, std::vector<long long>(K));
        fill_random_matrix(U);
        fill_random_matrix(V);
        fill_random_matrix(R);

        xK.resize(K);
        yK.resize(K);
        xN.resize(N);
        yN.resize(N);

        Vt.resize(N, std::vector<long long>(K));
        Vmasked.resize(N, std::vector<long long>(K));

        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < K; ++j) Vt[i][j] = V[i][j] + R[i][j];
        }
        eAlpha.resize(N);
    }
};

// vector dot product
long long vec_dot(const std::vector<long long> &a, const std::vector<long long> &b) {
    if (a.size() != b.size()) throw std::invalid_argument("Vectors must be same length for dot product.");
    long long out = 0;
    for (size_t i = 0; i < a.size(); ++i) out += a[i] * b[i];
    return out;
}

// elementwise vector addition
std::vector<long long> vec_add(const std::vector<long long> &a, const std::vector<long long> &b) {
    if (a.size() != b.size()) throw std::invalid_argument("Vectors must be same length for addition.");
    std::vector<long long> out(a.size());
    for (size_t i = 0; i < a.size(); ++i) out[i] = a[i] + b[i];
    return out;
}

// rotate a vector circularly by positions (can be negative)
std::vector<long long> circular_rotate(std::vector<long long> &v, long long positions) {
    int n = static_cast<int>(v.size());
    positions = (positions + n) % n;
    std::vector<long long> rotated(n);
    for (int i = 0; i < n; ++i) rotated[(i + positions) % n] = v[i];
    return rotated;
}

// compute share of V using e_i (binary selection vector) and masked V
std::vector<long long> compute_v_share(std::vector<long long> &e_i, std::vector<std::vector<long long>> &v_masked) {
    int n = static_cast<int>(v_masked.size());
    int k = static_cast<int>(v_masked[0].size());
    std::vector<long long> result(k, 0);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < k; ++j) {
            result[j] += e_i[i] * v_masked[i][j];
        }
    }
    return result;
}

// extract a column from a 2D matrix
std::vector<long long> column_extract(std::vector<std::vector<long long>> &matrix, int col_index) {
    std::vector<long long> col(matrix.size());
    for (size_t i = 0; i < matrix.size(); ++i) col[i] = matrix[i][col_index];
    return col;
}

// async I/O helpers (awaitable wrappers)
awaitable<void> async_send_scalar(tcp::socket &sock, long long &val) {
    co_await boost::asio::async_write(sock, boost::asio::buffer(&val, sizeof(val)), use_awaitable);
    co_return;
}

awaitable<void> async_recv_scalar(tcp::socket &sock, long long &val) {
    co_await boost::asio::async_read(sock, boost::asio::buffer(&val, sizeof(val)), use_awaitable);
    co_return;
}

awaitable<void> async_send_vector1d(tcp::socket &sock, std::vector<long long> &vec) {
    co_await boost::asio::async_write(sock, boost::asio::buffer(vec.data(), vec.size() * sizeof(long long)), use_awaitable);
    co_return;
}

awaitable<void> async_recv_vector1d(tcp::socket &sock, std::vector<long long> &vec) {
    co_await boost::asio::async_read(sock, boost::asio::buffer(vec.data(), vec.size() * sizeof(long long)), use_awaitable);
    co_return;
}

awaitable<void> async_send_matrix(tcp::socket &sock, std::vector<std::vector<long long>> &matrix) {
    for (auto &row : matrix) {
        co_await boost::asio::async_write(sock, boost::asio::buffer(row.data(), row.size() * sizeof(long long)), use_awaitable);
    }
    co_return;
}

awaitable<void> async_recv_matrix(tcp::socket &sock, std::vector<std::vector<long long>> &matrix) {
    for (auto &row : matrix) {
        co_await boost::asio::async_read(sock, boost::asio::buffer(row.data(), row.size() * sizeof(long long)), use_awaitable);
    }
    co_return;
}
