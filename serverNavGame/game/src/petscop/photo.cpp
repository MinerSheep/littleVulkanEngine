#include "petscop/photo.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace petscop {

const char* kPhotoFolder = "saves/photos";

namespace {

// A pixel written the way a colour is written everywhere else, six hex digits
std::string hexOf(const glm::vec3& colour) {
  auto step = [](float value) {
    const int byte = static_cast<int>(value * 255.f + 0.5f);
    return byte < 0 ? 0 : (byte > 255 ? 255 : byte);
  };

  char out[8];
  std::snprintf(out, sizeof(out), "%02x%02x%02x", step(colour.r), step(colour.g), step(colour.b));
  return out;
}

glm::vec3 colourOf(const std::string& hex) {
  if (hex.size() < 6) return glm::vec3(0.f);

  const long packed = std::strtol(hex.substr(0, 6).c_str(), nullptr, 16);
  return glm::vec3(((packed >> 16) & 0xff) / 255.f, ((packed >> 8) & 0xff) / 255.f,
                   (packed & 0xff) / 255.f);
}

// The number a filed picture carries, or -1 when the name is not one of ours
int numberOf(const std::string& name) {
  if (name.rfind("photo_", 0) != 0) return -1;
  return std::atoi(name.c_str() + 6);
}

}  // namespace

bool writePhoto(const std::string& path, const Photo& photo) {
  std::ofstream file(path);
  if (!file) return false;

  file << "photo 2\n";
  file << "room " << photo.room << "\n";
  file << "stamp " << photo.stamp << "\n";
  file << "size " << photo.picture.getWidth() << " " << photo.picture.getHeight() << "\n";

  for (int y = 0; y < photo.picture.getHeight(); y++) {
    file << "px";
    for (int x = 0; x < photo.picture.getWidth(); x++)
      file << " " << hexOf(photo.picture.isClear(x, y) ? glm::vec3(0.f) : photo.picture.at(x, y));
    file << "\n";
  }
  return true;
}

bool readPhoto(const std::string& path, Photo& photo) {
  std::ifstream file(path);
  if (!file) return false;

  photo = Photo();
  int wide = 0;
  int tall = 0;
  int row = 0;

  std::string line;
  while (std::getline(file, line)) {
    std::istringstream words(line);
    std::string key;
    if (!(words >> key)) continue;

    if (key == "room") {
      words >> photo.room;
    } else if (key == "stamp") {
      words >> photo.stamp;
    } else if (key == "size") {
      words >> wide >> tall;
      if (wide <= 0 || tall <= 0 || wide > 512 || tall > 512) return false;
      photo.picture.resize(wide, tall);
    } else if (key == "px") {
      if (row >= tall) continue;

      std::string hex;
      for (int x = 0; x < wide && (words >> hex); x++) photo.picture.set(x, row, colourOf(hex));
      row++;
    }
  }
  return !photo.room.empty() && !photo.picture.empty();
}

std::vector<std::string> photoFiles() {
  std::vector<std::string> found;

  DIR* folder = opendir(kPhotoFolder);
  if (!folder) return found;

  while (const dirent* entry = readdir(folder)) {
    const std::string name = entry->d_name;
    if (numberOf(name) < 0) continue;
    found.push_back(std::string(kPhotoFolder) + "/" + name);
  }
  closedir(folder);

  std::sort(found.begin(), found.end());
  return found;
}

bool filePhoto(const Photo& photo) {
  mkdir(kPhotoFolder, 0755);

  std::vector<std::string> already = photoFiles();

  // The folder never grows past its cap, and it is the oldest that goes
  while (static_cast<int>(already.size()) >= kPhotoLimit) {
    std::remove(already.front().c_str());
    already.erase(already.begin());
  }

  int next = 1;
  for (const std::string& path : already) {
    const std::size_t slash = path.rfind('/');
    next = std::max(next, numberOf(path.substr(slash + 1)) + 1);
  }

  char name[64];
  std::snprintf(name, sizeof(name), "%s/photo_%04d.txt", kPhotoFolder, next);
  return writePhoto(name, photo);
}

}  // namespace petscop
