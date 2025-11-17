#include <bits/stdc++.h>
#include <random>
#include <cstdint>
using namespace std;


// Returns a non-deterministic random 32-bit unsigned integer
uint32_t random_uint32(int min = 0, int max = INT32_MAX) {
    static std::random_device rd;            // True random seed
    static std::mt19937 gen(rd());           // Mersenne Twister engine
    static std::uniform_int_distribution<uint32_t> dist(min, max); // Uniform distribution in [min, max]
    return dist(gen);
}


int main(int argc, char* argv[]) {
    int n, m, k, Q;
    std::cout << "Enter number of items (n): ";
    std::cin >> n;
    std::cout << "Enter number of users (m): ";
    std::cin >> m;
    std::cout << "Enter number of features (k): ";
    std::cin >> k;
    std::cout << "Enter number of queries: ";
    std::cin >> Q;

    ofstream f1("files/input0.txt");
    ofstream f2("files/input1.txt");
    if (!f1 || !f2) {
        cerr << "Error opening output files.\n";
        return 1;
    }

    // Headers
    f1 << n << " " << m << " " << k << " " << Q << "\n";
    f2 << n << " " << m << " " << k << " " << Q << "\n";

    for (int q = 0; q < Q; q++) {
        // i in [0, m-1], j in [0, n-1]
        uint32_t i = random_uint32(0, m-1);
        uint32_t j = random_uint32(0, n-1);

        // Make additive shares: j = j0 + j1
        int j0 = static_cast<int>(random_uint32(0, n-1));
        int j1 = static_cast<int>(j) - j0;

        f1 << i << " " << j0 << "\n";
        f2 << i << " " << j1 << "\n";
    }

    f1.close();
    f2.close();

    cout << "Files input0.txt and input1.txt generated successfully.\n";
    return 0;
}
