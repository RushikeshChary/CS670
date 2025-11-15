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

struct correction_word
{
    uint32_t s_cw;
    bool f_cw0;
    bool f_cw1;
};

struct node
{
    uint32_t num;
    uint32_t S;
    bool f;
};

struct DPF_key
{
    node seed;
    node seed1;
    vector<correction_word> data;
    uint32_t final_cw0;
    uint32_t final_cw1;
};

// -------------------------------------------------------------
// Helper functions
// -------------------------------------------------------------
node get_node(uint32_t num)
{
    node nd;
    nd.num = num;
    nd.S = (num >> 1) & 0x7FFFFFFF;
    nd.f = num & 0x1;
    return nd;
}


uint32_t random_uint32()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dist(0, std::numeric_limits<uint32_t>::max());
    return dist(gen);
}

long long random_31bit_int()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<long long> dist(0, (1LL << 31) - 1);
    return dist(gen);
}

long long random_bit()
{ // Generate a random bit (0 or 1)
    static random_device rd;
    static mt19937_64 gen(rd());
    static uniform_int_distribution<long long> dist(0, 1);
    return dist(gen);
}

uint64_t PRG(uint32_t seed)
{
    std::mt19937_64 gen(static_cast<uint64_t>(seed));
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    return dist(gen);
}

void print_node(const node &nd, const string &label = "")
{
    if (!label.empty())
        cout << label << " ";
    cout << "(num=" << nd.num << ", S=" << nd.S << ", f=" << nd.f << ")\n";
}

void print_correction_word(const correction_word &cw, const string &label = "")
{
    if (!label.empty())
        cout << label << " ";
    cout << "[s_cw=" << cw.s_cw << ", f_cw0=" << cw.f_cw0 << ", f_cw1=" << cw.f_cw1 << "]\n";
}

void write_key_to_files(const DPF_key &key, ofstream &dpf0, ofstream &dpf1)
{
    dpf0 << key.seed.num << " " << key.seed.S << " " << key.seed.f << "\n";
    dpf1 << key.seed1.num << " " << key.seed1.S << " " << key.seed1.f << "\n";
    for (const auto &cw : key.data)
    {
        dpf0 << cw.s_cw << " " << cw.f_cw0 << "\n";
        dpf1 << cw.s_cw << " " << cw.f_cw1 << "\n";
    }
}

// -------------------------------------------------------------
// DPF generation
// -------------------------------------------------------------
DPF_key generateDPF(uint32_t index, long long value, uint32_t DPF_size)
{
    // summary << "\n[LOG] Starting generateDPF for index=" << index << ", value=" << value << "\n";

    DPF_key key;
    uint32_t depth = static_cast<uint32_t>(log2(DPF_size));
    vector<uint32_t> target(depth);
    for (int i = 0; i < depth; ++i)
        target[depth - 1 - i] = (index >> i) & 1;

    // summary << "[LOG] Target bits: ";
    // for (auto b : target)
    //     summary << b;
    // summary << "\n";

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

    // print_node(root, summary, "Initial root0:");
    // print_node(root1, summary, "Initial root1:");

    for (int i = 0; i < target.size(); i++)
    {
        // summary << "\n[LOG] === Level " << i << " === (bit=" << target[i] << ")\n";
        uint64_t prg_out = PRG(root.num);
        uint64_t prg_out1 = PRG(root1.num);
        node left, right, left1, right1;

        left.S = prg_out >> 33;
        left.f = (prg_out >> 32) & 1;
        left.num = (left.S << 1) | left.f;
        right.S = (prg_out >> 1) & ((1LL << 31) - 1);
        right.f = prg_out & 1;
        right.num = (right.S << 1) | right.f;

        left1.S = prg_out1 >> 33;
        left1.f = (prg_out1 >> 32) & 1;
        left1.num = (left1.S << 1) | left1.f;
        right1.S = (prg_out1 >> 1) & ((1LL << 31) - 1);
        right1.f = prg_out1 & 1;
        right1.num = (right1.S << 1) | right1.f;

        // summary << "[LOG] Before CW: \n";
        // print_node(left, summary, "  L0:");
        // print_node(right, summary, "  R0:");
        // print_node(left1, summary, "  L1:");
        // print_node(right1, summary, "  R1:");

        correction_word cw;
        if (target[i] == 0)
        {
            cw.s_cw = right.S ^ right1.S;
            cw.f_cw0 = left.f ^ left1.f ^ 1;
            cw.f_cw1 = right.f ^ right1.f;
            if (root.f)
            {
                left.S ^= cw.s_cw;
                right.S ^= cw.s_cw;
                left.f ^= cw.f_cw0;
                right.f ^= cw.f_cw1;
            }
            if (root1.f)
            {
                left1.S ^= cw.s_cw;
                right1.S ^= cw.s_cw;
                left1.f ^= cw.f_cw0;
                right1.f ^= cw.f_cw1;
            }
            left.num = (left.S << 1) | left.f;
            left1.num = (left1.S << 1) | left1.f;
            root = left;
            root1 = left1;
        }
        else
        {
            cw.s_cw = left.S ^ left1.S;
            cw.f_cw0 = left.f ^ left1.f;
            cw.f_cw1 = right.f ^ right1.f ^ 1;
            if (root.f)
            {
                left.S ^= cw.s_cw;
                right.S ^= cw.s_cw;
                left.f ^= cw.f_cw0;
                right.f ^= cw.f_cw1;
            }
            if (root1.f)
            {
                left1.S ^= cw.s_cw;
                right1.S ^= cw.s_cw;
                left1.f ^= cw.f_cw0;
                right1.f ^= cw.f_cw1;
            }
            right.num = (right.S << 1) | right.f;
            right1.num = (right1.S << 1) | right1.f;
            root = right;
            root1 = right1;
        }

        // summary << "[LOG] After CW:\n";
        // print_node(root, summary, "  Root0 ->");
        // print_node(root1, summary, "  Root1 ->");
        // print_correction_word(cw, summary, "  CW:");
        key.data.push_back(cw);
    }

    // uint32_t final_cw = (root.S ^ root1.S) ^ value;
    int flag = 1;
    if(root1.f == 1) flag = -1;
    uint32_t final_cw0 = flag * (value - root.S + root1.S) + 4;  // A constant offset is added to put some randomness.
    uint32_t final_cw1 = -4;
    key.final_cw0 = final_cw0;
    key.final_cw1 = final_cw1;

    // summary << "[LOG] Final CW added with s_cw=" << final_cw << "\n";

    // key.data = cw_vector;
    // summary << "[LOG] generateDPF completed.\n";
    // write_key_to_files(key, dpf0, dpf1);
    return key;
}

// -------------------------------------------------------------
// DPF Evaluation
// -------------------------------------------------------------
vector<int32_t> evalDPF(const node &seed, const vector<correction_word> &data, uint32_t fcw, uint32_t size, int party_id)
{
    // summary << "\n[LOG] Evaluating DPF tree of size " << size << "\n";
    vector<node> heap(2 * size - 1);
    heap[0] = seed;
    for (uint32_t i = 0; i < (size - 1); ++i)
    {
        uint64_t prg_out = PRG(heap[i].num);
        node left, right;
        left.S = prg_out >> 33;
        left.f = (prg_out >> 32) & 1;
        left.num = (left.S << 1) | left.f;
        right.S = (prg_out >> 1) & ((1LL << 31) - 1);
        right.f = prg_out & 1;
        right.num = (right.S << 1) | right.f;

        if (heap[i].f)
        {
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
    int flag = 1;
    if(party_id == 1) flag = -1;
    vector<int32_t> output(size);
    for (uint32_t i = 0; i < size; ++i)
    {
        uint32_t leaf_index = size - 1 + i;
        
        // if (heap[leaf_index].f)
        //     heap[leaf_index].S ^= data.back().s_cw;
        heap[leaf_index].S = flag * (heap[leaf_index].S + heap[leaf_index].f*fcw);
        output[i] = heap[leaf_index].S;
    }

    // summary << "[LOG] DPF evaluation done.\n";
    return output;
}

// // -------------------------------------------------------------
// // Full evaluation and correctness check
// // -------------------------------------------------------------
// bool EvalFull(const DPF_key &key, uint32_t target_index, long long target_value, ofstream &summary, uint32_t DPF_size)
// {
//     summary << "\n[LOG] Running EvalFull() for target_index=" << target_index
//          << ", target_value=" << target_value << "\n";

//     vector<uint32_t> arr = evalDPF(key.seed, key.data, key.final_cw0 + key.final_cw1, DPF_size, summary, 0);
//     vector<uint32_t> arr1 = evalDPF(key.seed1, key.data, key.final_cw0 + key.final_cw1, DPF_size, summary, 1);

//     bool test_passed = true;
//     for (uint32_t i = 0; i < DPF_size; ++i)
//     {
//         long long combined_value = arr[i] + arr1[i];
//         summary << "[LOG] i=" << i << ", combined=" << combined_value << "\n";
//         if (i == target_index)
//         {
//             if (combined_value != target_value)
//             {
//                 summary << "❌ Mismatch at target index " << i << " Expected " << target_value
//                      << " got " << combined_value << "\n";
//                 test_passed = false;
//             }
//         }
//         else
//         {
//             if (combined_value != 0)
//             {
//                 summary << "❌ Non-zero at index " << i << " value=" << combined_value << "\n";
//                 test_passed = false;
//             }
//         }
//     }

//     return test_passed;
// }

// // -------------------------------------------------------------
// // Main
// // -------------------------------------------------------------
// int main(int argc, char *argv[])
// {
//     if (argc < 3)
//     {
//         cerr << "Usage: " << argv[0] << " <DPF_size> <num_DPFs>\n";
//         return 1;
//     }

//     uint32_t DPF_size = stoi(argv[1]);
//     uint32_t num_DPFs = stoi(argv[2]);

//     if ((DPF_size <= 0) || ((DPF_size & (DPF_size - 1)) != 0))
//     {
//         cerr << "Error: DPF_size must be a power of two.\n";
//         return 1;
//     }

//     ofstream summary("summary.txt");
//     ofstream dpf0("dpf0_keys.txt");
//     ofstream dpf1("dpf1_keys.txt");
//     if (!summary || !dpf0 || !dpf1)
//     {
//         cerr << "Error opening output file.\n";
//         return 1;
//     }

//     for (int i = 0; i < num_DPFs; i++)
//     {
//         summary << "\n================= Generating DPF " << (i + 1) << " =================\n";
//         uint32_t index = random_uint32() % DPF_size;
//         long long value = random_31bit_int();
//         summary << "DPF " << i + 1 << ":\n";
//         summary << "Index: " << index << ", Value: " << value << "\n";

//         dpf0 << "DPF " << i + 1 << ":\n";
//         dpf1 << "DPF " << i + 1 << ":\n";
//         DPF_key key = generateDPF(index, value, dpf0, dpf1, summary, DPF_size);
//         dpf0 << "----------------------------------------------\n\n";
//         dpf1 << "----------------------------------------------\n\n";
//         if(EvalFull(key, index, value, summary, DPF_size))
//         {
//             summary << "DPF " << i + 1 << " passed.\n";
//             cout << "DPF " << i + 1 << " passed.\n";
//         }
//         else
//         {
//             summary << "DPF " << i + 1 << " failed.\n";
//             cout << "DPF " << i + 1 << " failed.\n";
//         }
//         cout << "Look in summary.txt for details.\n";
//     }

//     dpf0.close();
//     dpf1.close();
//     summary.close();
//     cout << "\n[LOG] All queries generated and tested.\n";
//     return 0;
// }
