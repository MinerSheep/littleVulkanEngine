// Checks X12: a photograph is filed, reads back the same, and the folder holds
//
// The picture itself is a copy of a frame the engine drew, which needs a live
// device, so that half is checked by taking one in the game. What is checked
// here is the file it goes into and the folder it goes in
//
// Build and run from the repo root:
//   g++ -std=c++17 -g3 -O0 -fsanitize=address -Iengine/include -Iengine/libs -Igame/src \
//       tools/test_photo.cpp game/src/petscop/photo.cpp engine/libvulkan_engine.a \
//       $(pkg-config --libs glfw3) -lvulkan -o /tmp/test_photo
//
//   /tmp/test_photo
//
// It says "all good" and exits 0, or prints what went wrong and exits 1

#include "petscop/photo.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void complain(const std::string& what) {
  std::cout << "  FAIL " << what << "\n";
  failures++;
}

// A picture with something different in every pixel, so a swap or a shift shows
petscop::Photo madeUp(const std::string& room) {
  petscop::Photo photo;
  photo.room = room;
  photo.stamp = 1788000000;
  photo.picture.resize(petscop::kPhotoWide, petscop::kPhotoTall);

  for (int y = 0; y < petscop::kPhotoTall; y++) {
    for (int x = 0; x < petscop::kPhotoWide; x++) {
      photo.picture.set(x, y, glm::vec3(x / 63.f, y / 47.f, ((x * 7 + y * 13) % 256) / 255.f));
    }
  }
  return photo;
}

// Two pictures the same, allowing for a colour going out as a byte and back
bool sameAs(const lve::LveCanvas& a, const lve::LveCanvas& b) {
  if (a.getWidth() != b.getWidth() || a.getHeight() != b.getHeight()) return false;

  for (int y = 0; y < a.getHeight(); y++) {
    for (int x = 0; x < a.getWidth(); x++) {
      const glm::vec3 gap = a.at(x, y) - b.at(x, y);
      if (std::fabs(gap.r) > 0.005f || std::fabs(gap.g) > 0.005f || std::fabs(gap.b) > 0.005f)
        return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  const petscop::Photo taken = madeUp("Foyer");

  // --- a photograph reads back as the one that was written -------------------
  const std::string scratch = "/tmp/test_photo_one.txt";
  if (!petscop::writePhoto(scratch, taken)) complain("the picture would not write");

  petscop::Photo read;
  if (!petscop::readPhoto(scratch, read)) complain("the picture would not read back");
  if (read.room != taken.room) complain("it came back as a different room");
  if (read.stamp != taken.stamp) complain("it came back with a different stamp");
  if (!sameAs(read.picture, taken.picture)) complain("the pixels came back changed");
  std::remove(scratch.c_str());

  // A file that is not a photograph is refused rather than half read
  if (petscop::readPhoto("/tmp/test_photo_missing.txt", read))
    complain("a picture that is not there read back anyway");

  // --- the folder ------------------------------------------------------------
  //
  // This writes into the real folder, so it only runs when there is nothing in
  // it to lose, and it takes back everything it put there
  if (!petscop::photoFiles().empty()) {
    printf("  (skipping the folder test, %s already has pictures in it)\n", petscop::kPhotoFolder);
  } else {
    for (int shot = 0; shot < 3; shot++) petscop::filePhoto(taken);

    std::vector<std::string> filed = petscop::photoFiles();
    if (filed.size() != 3)
      complain("three pictures filed and " + std::to_string(filed.size()) + " came back");

    // The newest is last, which is the end the viewer opens on
    if (filed.size() == 3 && filed[0] >= filed[2]) complain("the folder is not in order");

    // Filing past the cap drops the oldest rather than growing
    for (int shot = 0; shot < petscop::kPhotoLimit; shot++) petscop::filePhoto(taken);

    filed = petscop::photoFiles();
    if (static_cast<int>(filed.size()) != petscop::kPhotoLimit)
      complain("the folder holds " + std::to_string(filed.size()) + ", not " +
               std::to_string(petscop::kPhotoLimit));

    for (const std::string& gone : filed) std::remove(gone.c_str());
    if (!petscop::photoFiles().empty()) complain("the test left pictures behind");
  }

  printf("%s\n", failures == 0 ? "all good" : "SOMETHING IS WRONG");
  return failures == 0 ? 0 : 1;
}
