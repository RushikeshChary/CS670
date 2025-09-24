#include "common.hpp"

#if !defined(ROLE_p0) && !defined(ROLE_p1)
#error "ROLE must be defined as ROLE_p0 or ROLE_p1"
#endif

// Matrix structure for easy initialization, usage and randomization
struct Matrix {
    std::vector<std::vector<uint32_t>> data;

    Matrix(int rows, int cols) {
        data.assign(rows, std::vector<uint32_t>(cols));
    }

    uint32_t& operator()(int i, int j) { return data[i][j]; }
    const uint32_t& operator()(int i, int j) const { return data[i][j]; }

    int rows() const { return data.size(); }
    int cols() const { return data.empty() ? 0 : data[0].size(); }

    // Function to fill the matrix with random values
    void random_fill() {
        for (int i = 0; i < rows(); i++) {
            for (int j = 0; j < cols(); j++) {
                data[i][j] = random_uint32();
            }
        }
    }

    // Function to return column vector given column index
    std::vector<uint32_t> get_column(int col_idx) const {
        std::vector<uint32_t> col(rows());
        for (int i = 0; i < rows(); i++) {
            col[i] = data[i][col_idx];
        }
        return col;
    }

    // Function to print the matrix (for debugging)
    void print(const std::string& name) const {
        std::cout << name << " (" << rows() << "x" << cols() << "):\n";
        for (const auto& row : data) {
            for (auto val : row) std::cout << val << " ";
            std::cout << "\n";
        }
    }
};


// Structure to store input data and queries
struct InputData {
    int m, n, k, Q;
    std::vector<std::pair<int,int>> queries;
};

InputData read_file(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("Error opening file " + filename);
    }

    InputData data;
    in >> data.m >> data.n >> data.k >> data.Q;

    data.queries.resize(data.Q);
    for (int q = 0; q < data.Q; q++) {
        int i, jshare;
        in >> i >> jshare;
        data.queries[q] = {i, jshare};
    }
    return data;
}



// ----------------------- Helper coroutines -----------------------
awaitable<void> send_coroutine(tcp::socket& sock, uint32_t value) {
    co_await boost::asio::async_write(sock, boost::asio::buffer(&value, sizeof(value)), use_awaitable);
}

awaitable<void> recv_coroutine(tcp::socket& sock, uint32_t& out) {
    co_await boost::asio::async_read(sock, boost::asio::buffer(&out, sizeof(out)), use_awaitable);
}

// Blinded exchange between peers
awaitable<void> exchange_blinded(tcp::socket socket, uint32_t value_to_send) {
    uint32_t blinded = blind_value(value_to_send);
    uint32_t received;

    co_await send_coroutine(socket, blinded);
    co_await recv_coroutine(socket, received);

    std::cout << "Received blinded value from other party: " << received << std::endl;
    co_return;
}

// ----------------------- Setup connections -----------------------

// Setup connection to P2 (P0/P1 act as clients, P2 acts as server)
awaitable<tcp::socket> setup_server_connection(boost::asio::io_context& io_context, tcp::resolver& resolver) {
    tcp::socket sock(io_context);

    // Connect to P2
    auto endpoints_p2 = resolver.resolve("p2", "9002");
    co_await boost::asio::async_connect(sock, endpoints_p2, use_awaitable);

    co_return sock;
}

// Receive random value from P2
awaitable<uint32_t> recv_from_P2(tcp::socket& sock) {
    uint32_t received;
    co_await recv_coroutine(sock, received);
    co_return received;
}

// Setup peer connection between P0 and P1
awaitable<tcp::socket> setup_peer_connection(boost::asio::io_context& io_context, tcp::resolver& resolver) {
    tcp::socket sock(io_context);
#ifdef ROLE_p0
    auto endpoints_p1 = resolver.resolve("p1", "9001");
    co_await boost::asio::async_connect(sock, endpoints_p1, use_awaitable);
#else
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 9001));
    sock = co_await acceptor.async_accept(use_awaitable);
#endif
    co_return sock;
}

// ----------------------- Main protocol -----------------------
awaitable<void> run(boost::asio::io_context& io_context) {
    tcp::resolver resolver(io_context);

    // Step 1: connect to P2 and receive random value
    tcp::socket server_sock = co_await setup_server_connection(io_context, resolver);
    uint32_t received_from_p2 = co_await recv_from_P2(server_sock);

    std::cout << (
#ifdef ROLE_p0
        "P0"
#else
        "P1"
#endif
    ) << " received from P2: " << received_from_p2 << std::endl;

    // Step 2: connect to peer (P0 <-> P1)
    tcp::socket peer_sock = co_await setup_peer_connection(io_context, resolver);

    // Step 3: blinded exchange
    co_await exchange_blinded(std::move(peer_sock), received_from_p2);

    // Read input data from files
#ifdef ROLE_p0
    InputData input = read_file("f1.txt");
    Matrix U0(input.m, input.k);
    U0.random_fill();
    U0.print("U0");

    Matrix V0(input.n, input.k);
    V0.random_fill();
    V0.print("V0");

#else
    InputData input = read_file("f2.txt");
    Matrix U1(input.m, input.k);
    U1.random_fill();
    U1.print("U1");

    Matrix V1(input.n, input.k);
    V1.random_fill();
    V1.print("V1");
#endif

    co_return;
}

int main() {
    std::cout.setf(std::ios::unitbuf); // auto-flush cout for Docker logs
    boost::asio::io_context io_context(1);
    co_spawn(io_context, run(io_context), boost::asio::detached);
    io_context.run();
    return 0;
}
