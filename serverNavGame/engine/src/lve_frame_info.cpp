#include "lve_frame_info.hpp"
#include <unistd.h>
#include <limits.h>

std::string getExecutableDir() {
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer)-1);
    buffer[len] = '\0';
    std::string fullPath(buffer);
    return fullPath.substr(0, fullPath.find_last_of('/'));
}
