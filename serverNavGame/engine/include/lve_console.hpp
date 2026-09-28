#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace lve {

// Reads commands typed into the terminal the engine was launched from.
// A background thread only collects lines. Handlers always run on the thread
// that calls poll(), so they can touch engine state without any locking.
class LveConsole {
 public:
  using Args = std::vector<std::string>;
  // Returns text to print, or an empty string to print nothing.
  using Handler = std::function<std::string(const Args&)>;

  LveConsole();
  ~LveConsole();

  LveConsole(const LveConsole&) = delete;
  LveConsole& operator=(const LveConsole&) = delete;

  // name is matched case insensitively. usage is shown by the built in help command.
  void registerCommand(const std::string& name, const std::string& usage, Handler handler);

  void start();  // begins listening on stdin
  void stop();   // tells the listener to quit

  // Call once per frame on the main thread. Runs every command typed since the last call.
  void poll();

  // Argument helpers for handlers
  static bool parseBool(const std::string& text, bool& out);    // true/false/1/0/on/off/yes/no
  static bool parseFloat(const std::string& text, float& out);  // rejects junk and NaN/inf

 private:
  struct Shared {
    std::mutex mutex;
    std::queue<std::string> lines;
    std::atomic<bool> quit{false};
  };

  struct Command {
    std::string usage;
    Handler handler;
  };

  void execute(const std::string& line);

  // Shared with the listener thread so it can safely outlive this object
  // (getline cannot be interrupted, so the thread is detached).
  std::shared_ptr<Shared> shared;
  std::map<std::string, Command> commands;  // ordered so help prints alphabetically
  bool running = false;
};

}  // namespace lve
