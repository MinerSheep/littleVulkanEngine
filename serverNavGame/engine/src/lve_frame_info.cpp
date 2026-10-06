#include "lve_frame_info.hpp"
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

std::string getExecutableDir() {
    char buffer[PATH_MAX];
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(nullptr, buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';
#else
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) buffer[len] = '\0';
#endif
    std::string fullPath(buffer);
    return fullPath.substr(0, fullPath.find_last_of('/'));
}
