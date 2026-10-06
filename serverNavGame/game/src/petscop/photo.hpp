#pragma once

#include "lve_canvas.hpp"

#include <string>
#include <vector>

namespace petscop {

// A photograph is the frame that was on the screen when the shutter went
//
// The engine copies the finished frame aside and shrinks it, and what is filed
// is those pixels. Nothing is drawn again later -- it is the picture itself
struct Photo {
  std::string room;
  long long stamp = 0;
  lve::LveCanvas picture;
};

// How big a photograph is kept, which is what the viewer can afford to draw
const int kPhotoWide = 64;
const int kPhotoTall = 48;

// Where the pictures are filed, and the most the folder ever holds
extern const char* kPhotoFolder;
const int kPhotoLimit = 40;

bool writePhoto(const std::string& path, const Photo& photo);
bool readPhoto(const std::string& path, Photo& photo);

// Every picture in the folder, oldest first
std::vector<std::string> photoFiles();

// Files one, dropping the oldest once the folder is full
bool filePhoto(const Photo& photo);

}  // namespace petscop
