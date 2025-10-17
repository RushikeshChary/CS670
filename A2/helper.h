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

void print_node(const node &nd, ofstream &summary, const string &label = "")
{
    if (!label.empty())
        summary << label << " ";
    summary << "(num=" << nd.num << ", S=" << nd.S << ", f=" << nd.f << ")\n";
}

void print_correction_word(const correction_word &cw, ofstream &summary, const string &label = "")
{
    if (!label.empty())
        summary << label << " ";
    summary << "[s_cw=" << cw.s_cw << ", f_cw0=" << cw.f_cw0 << ", f_cw1=" << cw.f_cw1 << "]\n";
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