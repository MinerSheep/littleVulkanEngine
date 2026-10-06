#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

namespace lve {

// from: https://stackoverflow.com/a/57595105
template <typename T, typename... Rest>
void hashCombine(std::size_t& seed, const T& v, const Rest&... rest) {
  seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  (hashCombine(seed, rest), ...);
}

// Name of the person at the machine.
// On WSL, prefer the Windows login; otherwise use the native OS login.
// Looked up once and kept.
inline std::string desktopUserName() {
  static const std::string cached = [] {
#ifdef _WIN32
    char username[256] = {};
    DWORD usernameLength = static_cast<DWORD>(sizeof(username));

    if (GetUserNameA(username, &usernameLength) && username[0] != '\0') {
      return std::string(username);
    }

    // Fall back to the environment in case GetUserNameA fails.
    for (const char* key : {"USERNAME", "USER", "LOGNAME"}) {
      const char* value = std::getenv(key);
      if (value && *value) return std::string(value);
    }
#else
    const char* distro = std::getenv("WSL_DISTRO_NAME");
    if (distro && *distro) {
      FILE* pipe = popen("cmd.exe /C \"echo %USERNAME%\" 2>/dev/null", "r");
      if (pipe) {
        char line[128] = {};
        const bool got = fgets(line, sizeof(line), pipe) != nullptr;
        pclose(pipe);
        if (got) {
          std::string name(line);

          // cmd hands back a carriage return as well as a newline
          while (!name.empty() && (name.back() == '\n' || name.back() == '\r')) {
            name.pop_back();
          }

          // A leftover % means cmd never expanded it
          if (!name.empty() && name.find('%') == std::string::npos) {
            return name;
          }
        }
      }
    }

    for (const char* key : {"USER", "LOGNAME"}) {
      const char* value = std::getenv(key);
      if (value && *value) return std::string(value);
    }

    const passwd* record = getpwuid(geteuid());
    if (record && record->pw_name && *record->pw_name) {
      return std::string(record->pw_name);
    }
#endif

    return std::string("PLAYER");
  }();

  return cached;
}

}  // namespace lve
