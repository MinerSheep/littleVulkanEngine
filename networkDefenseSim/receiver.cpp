// Listens for Entity State PDUs and dead reckons each entity between updates.
// This is where you would hand positions to a renderer (Vulkan, Unity, anything).
#include "dis.hpp"
#include "net.hpp"
#include <chrono>
#include <map>
#include <tuple>

struct Track { dis::EntityStatePdu last; std::chrono::steady_clock::time_point rx; };

int main() {
    net_init();
    sock_t s = udp_socket();
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof yes);
    sockaddr_in me = make_addr("0.0.0.0", kDisPort);
    if (bind(s, reinterpret_cast<sockaddr*>(&me), sizeof me) < 0) { std::perror("bind"); return 1; }

#ifdef _WIN32
    DWORD tv = 100;
#else
    timeval tv{0, 100000};
#endif
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof tv);

    std::map<std::tuple<int,int,int>, Track> tracks;
    uint8_t buf[1500];
    std::printf("Listening on UDP %u...\n", kDisPort);

    for (;;) {
        int n = recv(s, reinterpret_cast<char*>(buf), sizeof buf, 0);
        auto now = std::chrono::steady_clock::now();
        if (n > 0) {
            dis::EntityStatePdu pdu;
            if (dis::decode(buf, size_t(n), pdu))
                tracks[{pdu.site, pdu.application, pdu.entity}] = {pdu, now};
        }
        for (auto& [id, tr] : tracks) {
            double dt = std::chrono::duration<double>(now - tr.rx).count();
            dis::Vec3d p = dis::extrapolate(tr.last, dt);
            std::printf("%-11s  y=%9.1f z=%9.1f  (age %.2fs)\n", tr.last.marking, p.y, p.z, dt);
        }
    }
}
