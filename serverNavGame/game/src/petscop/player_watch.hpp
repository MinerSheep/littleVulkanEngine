#pragma once

#include <glm/glm.hpp>

#include <string>

// Follows the player around and shouts on the console whenever he moves in a way
// walking and falling cannot account for
//
// A frame is handed over in three pieces -- where he stood, where the walk left
// him, where the pushes left him -- so a jolt can be blamed on the right one.
// Anything that stands him somewhere on purpose calls placed() instead
//
// Every line starts with [watch] and carries a frame number and a clock, so a
// "it went wrong about here" can be found again in the log
namespace petscop {

class PlayerWatch {
 public:
  // Where he stands before anything this frame has touched him
  void begin(const std::string& roomName, const glm::vec3& at, float dt);

  // The walk and the fall have run
  void afterMove(const glm::vec3& at, const glm::vec3& velocity, bool grounded);

  // The pushes have run, out of the box named here
  void afterSettle(const glm::vec3& at, const std::string& pushedBy);

  // Something stood him somewhere on purpose -- a door, a warp, a fall
  void placed(const std::string& roomName, const glm::vec3& at, const char* why);

  // What one step of walking is worth, so an unexplained step shows up
  float walkSpeed = 3.f;

  // How far a shove has to be before it is worth a line
  float shoveShout = 0.15f;

  // Seconds between the lines that print whatever else happened
  float heartbeat = 1.f;

  // Turn the whole thing off without pulling the calls back out
  bool on = true;

 private:
  // The frame number and clock every line opens with
  std::string head() const;

  std::string where;
  glm::vec3 started{0.f};
  glm::vec3 walked{0.f};
  float step = 0.f;
  float clock = 0.f;
  float sinceBeat = 0.f;
  long frame = 0;
};

}  // namespace petscop
