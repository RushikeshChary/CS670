#include <bits/stdc++.h>
#include <fstream>
using namespace std;

// Read matrix from file with dimensions
vector<vector<double>> read_matrix(ifstream &fin) {
    int m, n;
    fin >> m >> n;
    vector<vector<double>> mat(m, vector<double>(n));
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            fin >> mat[i][j];
        }
    }
    return mat;
}

// Reconstruct matrix: A = A0 + A1
vector<vector<double>> reconstruct(const vector<vector<double>> &a0,
                                   const vector<vector<double>> &a1) {
    int m = a0.size(), n = a0[0].size();
    vector<vector<double>> res(m, vector<double>(n));
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++)
            res[i][j] = a0[i][j] + a1[i][j];
    return res;
}

// Update rule: u_i <- u_i + v_j * (1 - <u_i, v_j>)
void update_row(vector<vector<double>> &U, const vector<vector<double>> &V, int i, int j) {
    int n = U[0].size();
    double dot = 0.0;
    for (int k = 0; k < n; k++) dot += U[i][k] * V[j][k];
    double delta = 1.0 - dot;
    for (int k = 0; k < n; k++) {
        U[i][k] = U[i][k] + V[j][k] * delta;
    }
}

// Compare two matrices with tolerance
bool equal_matrix(const vector<vector<double>> &A,
                  const vector<vector<double>> &B,
                  double eps = 1e-6) {
    if (A.size() != B.size() || A[0].size() != B[0].size()) return false;
    bool flag = true;
    for (int i = 0; i < (int)A.size(); i++)
        for (int j = 0; j < (int)A[0].size(); j++)
            if (fabs(A[i][j] - B[i][j]) > eps) return flag;
    return true;
}

// Print matrix to stdout
void print_matrix(const vector<vector<double>> &mat, std::ofstream &ofs, string name) {
    ofs << "=== " << name << " ===\n";
    for (const auto &row : mat) {
        for (double val : row) {
            ofs << val << " ";
        }
        ofs << "\n";
    }
    ofs << "\n";
    ofs << std::flush;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input_file>\n";
        return 1;
    }

    ifstream fin(argv[1]);
    if (!fin) {
        cerr << "Error: cannot open file " << argv[1] << endl;
        return 1;
    }
    string file = "check.txt";
    std::ofstream ofs(file);

    int i, j0, j1;
    fin >> i >> j0;
    fin >> i >> j1;
    int j = j0 + j1;

    auto u0 = read_matrix(fin);
    auto v0 = read_matrix(fin);
    auto u1 = read_matrix(fin);
    auto v1 = read_matrix(fin);
    auto u0_new = read_matrix(fin);
    auto u1_new = read_matrix(fin);

    // Reconstruct U, V, U'
    auto U = reconstruct(u0, u1);
    auto V = reconstruct(v0, v1);
    auto U_new = reconstruct(u0_new, u1_new);

    print_matrix(U, ofs, "U");
    print_matrix(U_new, ofs, "U_new");

    // Compute expected update
    auto U_expected = U;
    update_row(U_expected, V, i, j);
    print_matrix(U_expected, ofs, "U_expected");

    // Compare
    if (equal_matrix(U_expected, U_new)) {
        cout << "✅ Update is correct for user " << i << " and item " << j << endl;
    } else {
        cout << "❌ Update is incorrect\n";
        cout << "Expected row " << i << ": ";
        for (auto x : U_expected[i]) cout << x << " ";
        cout << "\nGot row " << i << ": ";
        for (auto x : U_new[i]) cout << x << " ";
        cout << "\n";
    }

    return 0;
}
