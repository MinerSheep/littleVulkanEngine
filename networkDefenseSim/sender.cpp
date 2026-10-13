// Simulates one aircraft flying a circle and publishes Entity State PDUs.
// Uses the DIS "drop threshold" idea: only send when dead reckoning error
// exceeds 1 m, or on a 5 s heartbeat.
#include "dis.hpp"
#include "net.hpp"
#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    const char* ip = argc > 1 ? argv[1] : "127.0.0.1";
    net_init();
    sock_t s = udp_socket();
    sockaddr_in dst = make_addr(ip, kDisPort);

    // Start on the equator-ish, flying a 2 km radius circle at 100 m/s (local tangent plane
    // approximated in ECEF for simplicity; swap in a real LLA->ECEF conversion later).
    const double R = 2000.0, speed = 100.0, omega = speed / R;
    const dis::Vec3d origin{6378137.0, 0.0, 0.0};

    dis::EntityStatePdu pdu;
    std::snprintf(pdu.marking, sizeof pdu.marking, "VIPER01");

    auto t0 = std::chrono::steady_clock::now();
    auto lastSent = t0;
    bool first = true;
    uint32_t sent = 0;

    // run indefinitely, sending PDUs when dead reckoning error exceeds 1 m or on a 5 s heartbeat
    for (;;) {
        auto now = std::chrono::steady_clock::now();
        double t = std::chrono::duration<double>(now - t0).count();

        // Ground truth
        dis::Vec3d truth{origin.x, origin.y + R * std::sin(omega * t), origin.z + R * (1 - std::cos(omega * t))};
        dis::Vec3f vel{0.f, float(speed * std::cos(omega * t)), float(speed * std::sin(omega * t))};

        double sinceSend = std::chrono::duration<double>(now - lastSent).count();
        dis::Vec3d predicted = dis::extrapolate(pdu, sinceSend);
        if (first || dis::distance(predicted, truth) > 1.0 || sinceSend > 5.0) {
            pdu.timestamp = uint32_t(t * 1000);
            pdu.location = truth;
            pdu.velocity = vel;
            auto buf = dis::encode(pdu);
            sendto(s, reinterpret_cast<const char*>(buf.data()), int(buf.size()), 0,
                   reinterpret_cast<sockaddr*>(&dst), sizeof dst);
            lastSent = now; first = false;
            std::printf("[send #%u] t=%.2f y=%.1f z=%.1f\n", ++sent, t, truth.y, truth.z);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));  // 50 Hz sim tick
    }
}
