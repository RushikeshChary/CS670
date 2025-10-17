#include <iostream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <fstream>
#include <bitset>
#include <climits>
using namespace std;

// -------------------------------------------------------------
// Utility structures
// -------------------------------------------------------------
uint32_t DPF_size = 16;
uint32_t num_DPFs = 10;

struct correction_word {
    uint32_t s_cw;
    bool f_cw0;
    bool f_cw1;
};

struct node {
    uint32_t num;
    uint32_t S;
    bool f;
};

struct DPF_key {
    node seed;
    node seed1;
    vector<correction_word> data;
};

// -------------------------------------------------------------
// Helper functions
// -------------------------------------------------------------
node get_node(uint32_t num) {
    node nd;
    nd.num = num;
    nd.S = (num >> 1) & 0x7FFFFFFF;
    nd.f = num & 0x1;
    return nd;
}

node get_left_child(uint64_t num) {
    node left;
    // uint32_t left_num = static_cast<uint32_t>(num >> 32);
    left.num = num;
    left.S = (num >> 33);
    left.f = (num >> 32) & 1;
    return left;
}

node get_right_child(uint64_t num) {
    node right;
    // uint32_t right_num = static_cast<uint32_t>(num & 0xFFFFFFFF);
    right.num = num;
    right.S = (num >> 1) & ((1LL << 31) - 1);
    right.f = (num & 1);
    return right;
}

uint32_t random_uint32() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dist(0, std::numeric_limits<uint32_t>::max());
    return dist(gen);
}

long long random_31bit_int() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<long long> dist(0, (1LL << 31) - 1);
    return dist(gen);
}

long long random_bit() { // Generate a random bit (0 or 1)
    static random_device rd;
    static mt19937_64 gen(rd());
    static uniform_int_distribution<long long> dist(0, 1);
    return dist(gen); 
}

uint64_t PRG(uint32_t seed) {
    std::mt19937_64 gen(static_cast<uint64_t>(seed));
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    return dist(gen);
}

void print_node(const node &nd, const string &label = "") {
    if (!label.empty()) cout << label << " ";
    cout << "(num=" << nd.num << ", S=" << nd.S << ", f=" << nd.f << ")\n";
}

void print_correction_word(const correction_word &cw, const string &label = "") {
    if (!label.empty()) cout << label << " ";
    cout << "[s_cw=" << cw.s_cw << ", f_cw0=" << cw.f_cw0 << ", f_cw1=" << cw.f_cw1 << "]\n";
}

// -------------------------------------------------------------
// DPF generation
// -------------------------------------------------------------
DPF_key generateDPF(uint32_t index, long long value) {
    cout << "\n[LOG] Starting generateDPF for index=" << index << ", value=" << value << "\n";

    DPF_key key;
    uint32_t depth = static_cast<uint32_t>(log2(DPF_size));
    vector<uint32_t> target(depth);
    for (int i = 0; i < depth; ++i)
        target[depth - 1 - i] = (index >> i) & 1;

    cout << "[LOG] Target bits: ";
    for (auto b : target) cout << b;
    cout << "\n";

    // vector<correction_word> cw_vector;
    node root, root1;
    root.S = random_31bit_int();
    root.f = random_bit();
    root.num = (root.S << 1) | root.f;

    root1.S = random_31bit_int();
    root1.f = root.f ^ 1;
    root1.num = (root1.S << 1) | root1.f;

    key.seed = root;
    key.seed1 = root1;

    print_node(root, "Initial root0:");
    print_node(root1, "Initial root1:");

    for (int i = 0; i < target.size(); i++) {
        cout << "\n[LOG] === Level " << i << " === (bit=" << target[i] << ")\n";
        uint64_t prg_out = PRG(root.num);
        uint64_t prg_out1 = PRG(root1.num);
        node left, right, left1, right1;
        // node left = get_left_child(prg_out);
        // node right = get_right_child(prg_out);
        // node left1 = get_left_child(prg_out1);
        // node right1 = get_right_child(prg_out1);
        left.S  = prg_out >> 33;
        left.f  = (prg_out >> 32) & 1;
        left.num = (left.S << 1) | left.f;
        right.S = (prg_out >> 1) & ((1LL << 31) - 1);
        right.f = prg_out & 1;
        right.num = (right.S << 1) | right.f;

        left1.S  = prg_out1 >> 33;
        left1.f  = (prg_out1 >> 32) & 1;
        left1.num = (left1.S << 1) | left1.f;
        right1.S = (prg_out1 >> 1) & ((1LL << 31) - 1);
        right1.f = prg_out1 & 1;
        right1.num = (right1.S << 1) | right1.f;

        cout << "[LOG] Before CW: \n";
        print_node(left, "  L0:");
        print_node(right, "  R0:");
        print_node(left1, "  L1:");
        print_node(right1, "  R1:");

        correction_word cw;
        if (target[i] == 0) {
            cw.s_cw = right.S ^ right1.S;
            cw.f_cw0 = left.f ^ left1.f ^ 1;
            cw.f_cw1 = right.f ^ right1.f;
        } else {
            cw.s_cw = left.S ^ left1.S;
            cw.f_cw0 = left.f ^ left1.f;
            cw.f_cw1 = right.f ^ right1.f ^ 1;
        }

        if(root.f) {
            left.S ^= cw.s_cw;
            right.S ^= cw.s_cw;
            left.f ^= cw.f_cw0;
            right.f ^= cw.f_cw1;
        }
        if(root1.f) {
            left1.S ^= cw.s_cw;
            right1.S ^= cw.s_cw;
            left1.f ^= cw.f_cw0;
            right1.f ^= cw.f_cw1;
        }

        if(target[i] == 0) {
            left.num = (left.S << 1) | left.f;
            left1.num = (left1.S << 1) | left1.f;
            root = left;
            root1 = left1;
        } else {
            right.num = (right.S << 1) | right.f;
            right1.num = (right1.S << 1) | right1.f;
            root = right;
            root1 = right1;
        }

        cout << "[LOG] After CW:\n";
        print_node(root, "  Root0 ->");
        print_node(root1, "  Root1 ->");
        print_correction_word(cw, "  CW:");
        key.data.push_back(cw);
    }

    uint32_t final_cw = (root.S ^ root1.S) ^ value;
    correction_word final_cw_struct;
    final_cw_struct.s_cw = final_cw;
    key.data.push_back(final_cw_struct);

    cout << "[LOG] Final CW added with s_cw=" << final_cw << "\n";

    // key.data = cw_vector;
    cout << "[LOG] generateDPF completed.\n";
    return key;
}

// -------------------------------------------------------------
// DPF Evaluation
// -------------------------------------------------------------
vector<uint32_t> evalDPF(const node &seed, const vector<correction_word> &data, uint32_t size) {
    cout << "\n[LOG] Evaluating DPF tree of size " << size << "\n";
    vector<node> heap(2 * size - 1);
    heap[0] = seed;
    for (uint32_t i = 0; i < (size - 1); ++i) {
        uint64_t prg_out = PRG(heap[i].num);
        node left, right;
        // node left = get_left_child(prg_out);
        // node right = get_right_child(prg_out);
        left.S  = prg_out >> 33;
        left.f  = (prg_out >> 32) & 1;
        left.num = (left.S << 1) | left.f;
        right.S = (prg_out >> 1) & ((1LL << 31) - 1);
        right.f = prg_out & 1;
        right.num = (right.S << 1) | right.f;

        if (heap[i].f) {
            uint32_t cw_index = static_cast<uint32_t>(log2(i + 1));
            left.S ^= data[cw_index].s_cw;
            left.f ^= data[cw_index].f_cw0;
            right.S ^= data[cw_index].s_cw;
            right.f ^= data[cw_index].f_cw1;

            left.num = (left.S << 1) | left.f;
            right.num = (right.S << 1) | right.f;
        }
        heap[2 * i + 1] = left;
        heap[2 * i + 2] = right;
    }

    vector<uint32_t> output(size);
    for (uint32_t i = 0; i < size; ++i) {
        uint32_t leaf_index = size - 1 + i;
        if (heap[leaf_index].f)
            heap[leaf_index].S ^= data.back().s_cw;
        output[i] = heap[leaf_index].S;
    }

    cout << "[LOG] DPF evaluation done.\n";
    return output;
}

// -------------------------------------------------------------
// Full evaluation and correctness check
// -------------------------------------------------------------
void EvalFull(const DPF_key &key, uint32_t target_index, long long target_value) {
    cout << "\n[LOG] Running EvalFull() for target_index=" << target_index
         << ", target_value=" << target_value << "\n";

    vector<uint32_t> arr = evalDPF(key.seed, key.data, DPF_size);
    vector<uint32_t> arr1 = evalDPF(key.seed1, key.data, DPF_size);

    // Print the outputs for debugging
    cout << "[LOG] Evaluated outputs:\n";
    for (uint32_t i = 0; i < DPF_size; ++i) {
        cout << "[LOG] i=" << i << ", arr=" << arr[i] << ", arr1=" << arr1[i] << "\n";
    }
    cout<<endl;

    bool test_passed = true;
    for (uint32_t i = 0; i < DPF_size; ++i) {
        long long combined_value = arr[i] ^ arr1[i];
        cout << "[LOG] i=" << i << ", combined=" << combined_value << "\n";
        if (i == target_index) {
            if (combined_value != target_value) {
                cout << "❌ Mismatch at target index " << i << " Expected " << target_value
                     << " got " << combined_value << "\n";
                test_passed = false;
            }
        } else {
            if (combined_value != 0) {
                cout << "❌ Non-zero at index " << i << " value=" << combined_value << "\n";
                test_passed = false;
            }
        }
    }

    if (test_passed)
        cout << "✅ Test Passed\n";
    else
        cout << "❌ Test Failed\n";
}

// -------------------------------------------------------------
// Main
// -------------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <DPF_size> <num_DPFs>\n";
        return 1;
    }

    DPF_size = stoi(argv[1]);
    num_DPFs = stoi(argv[2]);

    if ((DPF_size <= 0) || ((DPF_size & (DPF_size - 1)) != 0)) {
        cerr << "Error: DPF_size must be a power of two.\n";
        return 1;
    }

    ofstream f("queries.txt");
    if (!f) {
        cerr << "Error opening output file.\n";
        return 1;
    }

    for (int i = 0; i < num_DPFs; i++) {
        cout << "\n================= Generating DPF " << (i + 1) << " =================\n";
        uint32_t index = random_uint32() % DPF_size;
        long long value = random_31bit_int();
        f << "DPF " << i + 1 << ":\n";
        f << "Index: " << index << ", Value: " << value << "\n";

        DPF_key key = generateDPF(index, value);
        EvalFull(key, index, value);
    }

    f.close();
    cout << "\n[LOG] All queries generated and tested.\n";
    return 0;
}
