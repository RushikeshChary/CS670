// Define a structure for shares in shares.h for easy initialization and randomization.

#include "common.hpp"
class Share {
    public:
        int n, m, k; // number of items, users, features
        std::vector<std::vector<int>> u, v, r, v_dash, v_masked, x_n, y_n;
        std::vector<int> x_k, y_k, e_alpha, scaler_x, scaler_y, scaler_gamma, gamma_n;
        int gamma_k;
        int alpha;

    Share(int n, int m, int k) : n(n), m(m), k(k) {
        u.resize(m, std::vector<int>(k));
        v.resize(n, std::vector<int>(k));
        r.resize(n, std::vector<int>(k));
        x_n.resize(n, std::vector<int>(k));
        y_n.resize(n, std::vector<int>(k));
        fill_random(u);
        fill_random(v);
        fill_random(r);
        x_k.resize(k);
        y_k.resize(k);
        scaler_x.resize(k);
        scaler_y.resize(k);
        scaler_gamma.resize(k);
        gamma_n.resize(k);
        v_dash.resize(n, std::vector<int>(k));
        v_masked.resize(n, std::vector<int>(k));
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < k; j++) v_dash[i][j] = v[i][j] + r[i][j];
        }
        e_alpha.resize(n);
    }
};