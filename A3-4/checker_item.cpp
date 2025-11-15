#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
using namespace std;

// ---------------- Basic Utilities ----------------
vector<int> parse_vector(const string& line) {
    vector<int> v;
    stringstream ss(line);
    int x;
    while (ss >> x) v.push_back(x);
    return v;
}

vector<vector<int>> parse_matrix(ifstream& fin, int rows, int cols) {
    vector<vector<int>> M(rows, vector<int>(cols));
    string line;
    for (int i = 0; i < rows; i++) {
        getline(fin, line);
        M[i] = parse_vector(line);
    }
    return M;
}

void print_vec(const vector<int>& v, string name="") {
    if (!name.empty()) cout << name << ": ";
    for (int x : v) cout << x << " ";
    cout << "\n";
}

void print_matrix(const vector<vector<int>>& M, string name="") {
    cout << "\n" << name << " (" << M.size() << "x" << (M.empty()?0:M[0].size()) << "):\n";
    for (auto &row : M) {
        for (int x : row) cout << x << " ";
        cout << "\n";
    }
    cout << "\n";
}

// ---- Math helpers ----
int dot(const vector<int>& a, const vector<int>& b) {
    int s = 0;
    for (int i = 0; i < (int)a.size(); ++i) s += a[i] * b[i];
    return s;
}

vector<int> add(const vector<int>& a, const vector<int>& b) {
    vector<int> r(a.size());
    for (int i = 0; i < (int)a.size(); ++i) r[i] = a[i] + b[i];
    return r;
}

vector<int> mul_scalar(const vector<int>& a, int s) {
    vector<int> r(a.size());
    for (int i = 0; i < (int)a.size(); ++i) r[i] = a[i] * s;
    return r;
}

// ---------------- Correctness Check ----------------
void check_item(
    const vector<vector<int>>& V0,
    const vector<vector<int>>& V1,
    const vector<vector<int>>& V0_upd,
    const vector<vector<int>>& V1_upd,
    const vector<int>& u,
    int j
) {
    int k = u.size();

    // reconstruct old
    vector<int> v_old(k);
    for (int t = 0; t < k; t++)
        v_old[t] = V0[j][t] + V1[j][t];

    // compute expected update
    int dp = dot(u, v_old);
    int delta = 1 - dp;
    vector<int> M = mul_scalar(u, delta);
    vector<int> v_expected = add(v_old, M);

    // reconstruct actual updated
    vector<int> v_actual(k);
    for (int t = 0; t < k; t++)
        v_actual[t] = V0_upd[j][t] + V1_upd[j][t];

    // Print
    cout << "---------------------------------------------------------\n";
    cout << " Checking correctness for item index j = " << j << "\n";
    cout << "---------------------------------------------------------\n";

    print_vec(v_old, "Old v_j");
    print_vec(M, "Update term M = u*(1 - <u, v_j>)");
    print_vec(v_expected, "Expected updated v_j");
    print_vec(v_actual, "Actual updated v_j (from shares)");

    // Compare
    bool ok = true;
    for (int t = 0; t < k; t++) {
        if (v_expected[t] != v_actual[t]) {
            ok = false;
            cout << "❌ Mismatch at pos " << t
                 << ": expected " << v_expected[t]
                 << ", got " << v_actual[t] << "\n";
        }
    }

    if (ok) cout << "\n✅ Update is CORRECT for item index j = " << j << "\n";
    else    cout << "\n❌ Update is WRONG for item index j = " << j << "\n";
}

// ---------------- Main: reads from file ----------------
int main() {
    ifstream fin("check_item.txt");
    if (!fin) {
        cout << "Error: check_item.txt not found\n";
        return 1;
    }

    string line;

    // ---------------- Read inputs ----------------
    getline(fin, line);
    int j = stoi(line);

    getline(fin, line);
    int k = stoi(line);

    getline(fin, line);  // "u: ..."
    line = line.substr(line.find(":")+1);
    vector<int> u = parse_vector(line);

    // Read all remaining lines
    vector<string> buffer;
    while (getline(fin, line)) buffer.push_back(line);

    // Section finders
    auto find_index = [&](const string& key){
        for (int i = 0; i < (int)buffer.size(); i++)
            if (buffer[i].find(key) != string::npos)
                return i + 1;
        return -1;
    };

    int iV0   = find_index("V0:");
    int iV1   = find_index("V1:");
    int iV0u  = find_index("V0_updated:");
    int iV1u  = find_index("V1_updated:");

    int rows = (iV1 - iV0) - 1; // #rows in matrix

    auto read_block = [&](int start){
        vector<vector<int>> M(rows, vector<int>(k));
        for (int r = 0; r < rows; r++)
            M[r] = parse_vector(buffer[start + r]);
        return M;
    };

    vector<vector<int>> V0     = read_block(iV0);
    vector<vector<int>> V1     = read_block(iV1);
    vector<vector<int>> V0_upd = read_block(iV0u);
    vector<vector<int>> V1_upd = read_block(iV1u);

    // ---------------- Print everything ----------------
    cout << "========== LOADED INPUT ==========\n\n";
    cout << "Item index j = " << j << "\n";
    cout << "k (dimension) = " << k << "\n";
    print_vec(u, "User vector u");

    print_matrix(V0, "V0 (old share)");
    print_matrix(V1, "V1 (old share)");
    print_matrix(V0_upd, "V0_updated");
    print_matrix(V1_upd, "V1_updated");

    // ---------------- Perform correctness check ----------------
    check_item(V0, V1, V0_upd, V1_upd, u, j);

    return 0;
}
