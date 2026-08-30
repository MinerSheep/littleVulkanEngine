#include "petscop/save_file.hpp"

#include <sys/stat.h>

#include <fstream>
#include <iomanip>
#include <sstream>

// The save is plain text, one fact per line, in the same spirit as a .map
//
//   save 1
//   room foyer
//   flag chest_open
//   item red_key 1
//   prop foyer.gate  5.300 -3.250 -1.500  0 0 0  0.250 1.150 0.900  hidden 0  flips 1 0
//
// A line it does not recognise is skipped rather than refused, and a save from an
// older version is ignored outright
namespace petscop {

namespace {

const int saveVersion = 1;

// Makes every folder ahead of the file name
void makeFolders(const std::string& path) {
  for (std::size_t i = 1; i < path.size(); i++) {
    if (path[i] == '/') mkdir(path.substr(0, i).c_str(), 0755);
  }
}

bool readVec3(std::istringstream& ss, glm::vec3& out) {
  return static_cast<bool>(ss >> out.x >> out.y >> out.z);
}

void writeVec3(std::ofstream& out, const glm::vec3& value) {
  out << value.x << " " << value.y << " " << value.z;
}

// Reads one remembered prop off the rest of its line
bool readMemory(std::istringstream& ss, PropMemory& memory) {
  if (!readVec3(ss, memory.translation) || !readVec3(ss, memory.rotation) ||
      !readVec3(ss, memory.scale))
    return false;

  std::string keyword;
  int hidden = 0;
  if (!(ss >> keyword >> hidden)) return false;
  memory.hidden = hidden != 0;

  if (!(ss >> keyword)) return true;  // no flips on the line at all

  int flip = 0;
  while (ss >> flip) memory.flipped.push_back(flip != 0 ? 1 : 0);
  return true;
}

}  // namespace

bool readSave(const std::string& path, GameState& state) {
  std::ifstream in(path);
  if (!in) return false;

  state = GameState{};

  std::string line;
  bool versionSeen = false;

  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::string key;
    if (!(ss >> key)) continue;

    if (key == "save") {
      int version = 0;
      if (!(ss >> version) || version != saveVersion) {
        state = GameState{};
        return false;
      }
      versionSeen = true;

    } else if (key == "room") {
      ss >> state.room;

    } else if (key == "flag") {
      std::string name;
      if (ss >> name) state.flags.insert(name);

    } else if (key == "item") {
      std::string name;
      int count = 0;
      if ((ss >> name >> count) && count > 0) state.items[name] = count;

    } else if (key == "played") {
      ss >> state.lastPlayed;

    } else if (key == "other") {
      // Where he is, what he has, and what he did while the game was shut
      std::string what;
      if (!(ss >> what)) continue;

      if (what == "room") {
        ss >> state.other.room;
      } else if (what == "seed") {
        ss >> state.other.seed;
      } else if (what == "item") {
        std::string name;
        int count = 0;
        if ((ss >> name >> count) && count > 0) state.other.pocket[name] = count;
      } else if (what == "did") {
        std::string words;
        std::getline(ss, words);
        if (!words.empty() && words[0] == ' ') words.erase(0, 1);
        if (!words.empty()) state.other.trace.push_back(words);
      }

    } else if (key == "prop") {
      std::string name;
      PropMemory memory;
      if ((ss >> name) && readMemory(ss, memory)) state.memories[name] = memory;
    }
  }

  if (!versionSeen) {
    state = GameState{};
    return false;
  }
  return true;
}

bool writeSave(const std::string& path, const GameState& state) {
  makeFolders(path);

  std::ofstream out(path);
  if (!out) return false;

  out << std::fixed << std::setprecision(3);
  out << "save " << saveVersion << "\n";
  if (!state.room.empty()) out << "room " << state.room << "\n";
  if (state.lastPlayed > 0) out << "played " << state.lastPlayed << "\n";

  if (!state.other.room.empty()) out << "other room " << state.other.room << "\n";
  if (state.other.seed != 0) out << "other seed " << state.other.seed << "\n";
  for (const std::pair<const std::string, int>& held : state.other.pocket)
    out << "other item " << held.first << " " << held.second << "\n";
  for (const std::string& line : state.other.trace) out << "other did " << line << "\n";

  for (const std::string& flag : state.flags) out << "flag " << flag << "\n";

  for (const std::pair<const std::string, int>& item : state.items)
    out << "item " << item.first << " " << item.second << "\n";

  for (const std::pair<const std::string, PropMemory>& entry : state.memories) {
    const PropMemory& memory = entry.second;
    out << "prop " << entry.first << "  ";
    writeVec3(out, memory.translation);
    out << "  ";
    writeVec3(out, memory.rotation);
    out << "  ";
    writeVec3(out, memory.scale);
    out << "  hidden " << (memory.hidden ? 1 : 0) << "  flips";
    for (char flip : memory.flipped) out << " " << (flip != 0 ? 1 : 0);
    out << "\n";
  }

  return static_cast<bool>(out);
}

}  // namespace petscop
