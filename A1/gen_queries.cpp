#include <bits/stdc++.h>
#include <common.hpp>
using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 5) {
        cerr << "Usage: " << argv[0] << " m n k Q\n";
        return 1;
    }

    int m = stoi(argv[1]);
    int n = stoi(argv[2]);
    int k = stoi(argv[3]);
    int Q = stoi(argv[4]);

    ofstream f1("f1.txt");
    ofstream f2("f2.txt");
    if (!f1 || !f2) {
        cerr << "Error opening output files.\n";
        return 1;
    }

    // Headers
    f1 << m << " " << n << " " << k << " " << Q << "\n";
    f2 << m << " " << n << " " << k << " " << Q << "\n";

    for (int q = 0; q < Q; q++) {
        // i in [0, m-1], j in [0, n-1]
        uint32_t i = random_uint32() % m;
        uint32_t j = random_uint32() % n;

        // Make additive shares: j = j0 + j1
        int j0 = static_cast<int>(random_uint32() % (2 * n)); // spread more
        int j1 = static_cast<int>(j) - j0;

        f1 << i << " " << j0 << "\n";
        f2 << i << " " << j1 << "\n";
    }

    f1.close();
    f2.close();

    cout << "Files f1.txt and f2.txt generated successfully.\n";
    return 0;
}
