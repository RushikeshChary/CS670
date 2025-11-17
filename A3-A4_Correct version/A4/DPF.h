// key_renamed.h
#include <iostream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <fstream>
#include <bitset>
#include <climits>
#include <vector>
#include <cmath>

using namespace std;

struct TreeNode { // store seed and flag for each node in DPF tree
    long long seed;
    long long flag;
};

struct CorrWord { // correction word per level
    long long seed_cw;
    long long flag_cw0;
    long long flag_cw1;
};

struct DPFKey { // a DPF key
    long long seed;
    long long flag;
    long long final_cw;
    long long sign;
    vector<CorrWord> cw_list;
};

long long rng16_from_seed(long long seed) { // deterministic 16-bit value from seed
    std::mt19937_64 gen(static_cast<long long>(seed));
    std::uniform_int_distribution<long long> dist(0, (1LL << 16) - 1);
    return dist(gen);
}

long long rng7() { // random 7-bit value
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<long long> dist(0, (1LL << 7) - 1);
    return dist(gen);
}

long long rng_bit() { // random bit 0/1
    static random_device rd;
    static mt19937_64 gen(rd());
    static uniform_int_distribution<long long> dist(0, 1);
    return dist(gen);
}

long long rng_upto(long long n) { // random in [0, n]
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<long long> dist(0, n);
    return dist(gen);
}

string index_to_binstr(long long idx, int space_size) { // binary string length = log2(space_size)
    string bin;
    while (idx > 0) {
        bin = (idx % 2 == 0 ? "0" : "1") + bin;
        idx /= 2;
    }
    int length = static_cast<int>(log2(space_size));
    while ((int)bin.size() < length) bin = "0" + bin;
    return bin;
}

// We are going to implement generateDPF and evalDPF functions using heap datastructure to store the tree level-wise in a compact manner.
void generateDPF(long long target_index, long long message_value, int domain_size,
                        std::vector<long long> &out_key_vec0, std::vector<long long> &out_key_vec1) {
    DPFKey k0, k1;

    int pow2 = 1;
    while (pow2 < domain_size) pow2 *= 2;
    domain_size = pow2;

    k0.seed = rng7();
    k1.seed = rng7();
    k0.flag = rng_bit();
    k1.flag = (k0.flag == 1) ? 0 : 1;

    string path = index_to_binstr(target_index, domain_size);

    long long state0 = k0.seed * 2 + k0.flag;
    long long state1 = k1.seed * 2 + k1.flag;

    for (int level = 0; level < (int)path.size(); ++level) {
        long long out0 = rng16_from_seed(state0);
        long long out1 = rng16_from_seed(state1);

        long long s0_left = out0 >> 9;
        long long f0_left = (out0 >> 8) & 1;
        long long s0_right = (out0 >> 1) & ((1LL << 7) - 1);
        long long f0_right = out0 & 1;

        long long s1_left = out1 >> 9;
        long long f1_left = (out1 >> 8) & 1;
        long long s1_right = (out1 >> 1) & ((1LL << 7) - 1);
        long long f1_right = out1 & 1;

        CorrWord cw;
        if (path[level] == '0') {
            cw.seed_cw = s0_right ^ s1_right;
            cw.flag_cw0 = (f0_left ^ f1_left) ^ 1;
            cw.flag_cw1 = f0_right ^ f1_right;
        } else {
            cw.seed_cw = s0_left ^ s1_left;
            cw.flag_cw0 = f0_left ^ f1_left;
            cw.flag_cw1 = (f0_right ^ f1_right) ^ 1;
        }

        if ((state0 & 1) == 1) { // apply cw to children for party 0 if needed
            s0_left ^= cw.seed_cw;
            s0_right ^= cw.seed_cw;
            f0_left ^= cw.flag_cw0;
            f0_right ^= cw.flag_cw1;
        }
        if ((state1 & 1) == 1) { // apply cw to children for party 1 if needed
            s1_left ^= cw.seed_cw;
            s1_right ^= cw.seed_cw;
            f1_left ^= cw.flag_cw0;
            f1_right ^= cw.flag_cw1;
        }

        if (path[level] == '0') {
            state0 = s0_left * 2 + f0_left;
            state1 = s1_left * 2 + f1_left;
        } else {
            state0 = s0_right * 2 + f0_right;
            state1 = s1_right * 2 + f1_right;
        }

        k0.cw_list.push_back(cw);
        k1.cw_list.push_back(cw);
    }

    k0.final_cw = rng7();
    k1.final_cw = ((state1 & 1) ? -1 : 1) * (message_value - (state0 >> 1) + (state1 >> 1)) - k0.final_cw;

    k0.sign = (state1 & 1);
    k1.sign = (state1 & 1);

    out_key_vec0.push_back(k0.sign);
    out_key_vec0.push_back(k0.seed);
    out_key_vec0.push_back(k0.flag);
    out_key_vec0.push_back(k0.final_cw);
    for (auto &cw : k0.cw_list) {
        out_key_vec0.push_back(cw.seed_cw);
        out_key_vec0.push_back(cw.flag_cw0);
        out_key_vec0.push_back(cw.flag_cw1);
    }

    out_key_vec1.push_back(k1.sign);
    out_key_vec1.push_back(k1.seed);
    out_key_vec1.push_back(k1.flag);
    out_key_vec1.push_back(k1.final_cw);
    for (auto &cw : k1.cw_list) {
        out_key_vec1.push_back(cw.seed_cw);
        out_key_vec1.push_back(cw.flag_cw0);
        out_key_vec1.push_back(cw.flag_cw1);
    }
}

vector<long long> evalDPF(DPFKey &dpf_key, int domain_size, long long party_id, long long FCW_value) {
    int original_size = domain_size;
    int pow2 = 1;
    while (pow2 < domain_size) pow2 *= 2;
    domain_size = pow2;

    vector<TreeNode> tree(2 * domain_size - 1);
    tree[0].seed = dpf_key.seed;
    tree[0].flag = dpf_key.flag;

    for (int idx = 0; idx < domain_size - 1; ++idx) {
        long long parent_state = tree[idx].seed * 2 + tree[idx].flag;
        long long branch = rng16_from_seed(parent_state);
        tree[2*idx + 1].seed = branch >> 9;
        tree[2*idx + 1].flag = (branch >> 8) & 1;
        tree[2*idx + 2].seed = (branch >> 1) & ((1LL << 7) - 1);
        tree[2*idx + 2].flag = branch & 1;

        if (tree[idx].flag == 1) {
            int lvl = static_cast<int>(log2(idx + 1));
            tree[2*idx + 1].seed ^= dpf_key.cw_list[lvl].seed_cw;
            tree[2*idx + 2].seed ^= dpf_key.cw_list[lvl].seed_cw;
            tree[2*idx + 1].flag ^= dpf_key.cw_list[lvl].flag_cw0;
            tree[2*idx + 2].flag ^= dpf_key.cw_list[lvl].flag_cw1;
        }
    }

    for (int leaf = domain_size - 1; leaf < 2 * domain_size - 1; ++leaf) {
        tree[leaf].seed = (party_id ? -1 : 1) * (tree[leaf].seed + tree[leaf].flag * FCW_value);
    }

    vector<long long> output(original_size);
    for (int i = 0; i < original_size; ++i) {
        output[i] = tree[i + domain_size - 1].seed;
    }
    return output;
}

void build_key_from_vector(std::vector<long long> &vec, DPFKey &out_key) {
    out_key.sign = vec[0];
    out_key.seed = vec[1];
    out_key.flag = vec[2];
    out_key.final_cw = vec[3];
    int cw_count = (static_cast<int>(vec.size()) - 4) / 3;
    out_key.cw_list.clear();
    for (int i = 0; i < cw_count; ++i) {
        CorrWord cw;
        cw.seed_cw = vec[4 + 3*i];
        cw.flag_cw0 = vec[5 + 3*i];
        cw.flag_cw1 = vec[6 + 3*i];
        out_key.cw_list.push_back(cw);
    }
}
