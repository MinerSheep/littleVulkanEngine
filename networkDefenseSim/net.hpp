// Tiny UDP socket wrapper (Winsock or POSIX).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using sock_t = SOCKET;
  inline void net_init() { WSADATA d; if (WSAStartup(MAKEWORD(2,2), &d)) std::exit(1); }
  inline void net_close(sock_t s) { closesocket(s); }
#else
  #include <arpa/inet.h>
  #include <sys/socket.h>
  #include <unistd.h>
  using sock_t = int;
  inline void net_init() {}
  inline void net_close(sock_t s) { close(s); }
#endif

constexpr uint16_t kDisPort = 3000;  // common DIS default

inline sock_t udp_socket() {
    sock_t s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == (sock_t)-1) { std::perror("socket"); std::exit(1); }
    return s;
}

inline sockaddr_in make_addr(const char* ip, uint16_t port) {
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port);
    inet_pton(AF_INET, ip, &a.sin_addr);
    return a;
}
