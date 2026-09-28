#include "lve_console.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <thread>

namespace lve {

namespace {
std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}
}  // namespace

LveConsole::LveConsole() : shared(std::make_shared<Shared>()) {
  registerCommand("help", "help", [this](const Args&) {
    std::ostringstream out;
    out << "Commands:";
    for (const auto& [name, cmd] : commands) out << "\n  " << cmd.usage;
    return out.str();
  });
}

LveConsole::~LveConsole() { stop(); }

void LveConsole::registerCommand(const std::string& name, const std::string& usage, Handler handler) {
  // on a scene change, these console commands need to be DISABLED
  commands[toLower(name)] = Command{usage, std::move(handler)};
}

void LveConsole::start() {
  if (running) return;
  running = true;

  auto state = shared;  // the thread keeps its own reference
  std::thread([state]() {
    std::string line;
    while (!state->quit && std::getline(std::cin, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();  // Windows terminals
      std::lock_guard<std::mutex> lock(state->mutex);
      state->lines.push(line);
    }
  }).detach();

  std::cout << "Console ready. Type help for a list of commands." << std::endl;
}

void LveConsole::stop() {
  shared->quit = true;
  running = false;
}

void LveConsole::poll() {
  std::queue<std::string> pending;
  {
    std::lock_guard<std::mutex> lock(shared->mutex);
    std::swap(pending, shared->lines);
  }
  while (!pending.empty()) {
    execute(pending.front());
    pending.pop();
  }
}

void LveConsole::execute(const std::string& line) {
  std::istringstream stream(line);
  std::string name;
  if (!(stream >> name)) return;  // blank line

  Args args;
  for (std::string word; stream >> word;) args.push_back(word);

  auto it = commands.find(toLower(name));
  if (it == commands.end()) {
    std::cout << "Unknown command: " << name << " (type help)" << std::endl;
    return;
  }

  try {
    const std::string result = it->second.handler(args);
    if (!result.empty()) std::cout << result << std::endl;
  } catch (const std::exception& e) {
    std::cout << "Command failed: " << e.what() << std::endl;
  }
}

bool LveConsole::parseBool(const std::string& text, bool& out) {
  const std::string s = toLower(text);
  if (s == "true" || s == "1" || s == "on" || s == "yes") {
    out = true;
    return true;
  }
  if (s == "false" || s == "0" || s == "off" || s == "no") {
    out = false;
    return true;
  }
  return false;
}

bool LveConsole::parseFloat(const std::string& text, float& out) {
  if (text.empty()) return false;
  char* end = nullptr;
  const float value = std::strtof(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !std::isfinite(value)) return false;
  out = value;
  return true;
}

}  // namespace lve
