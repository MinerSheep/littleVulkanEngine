#include "petscop/player_watch.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace petscop {

namespace {

// A position the way every line prints it
std::string vec(const glm::vec3& v) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2) << "(" << std::setw(7) << v.x << "," << std::setw(7)
      << v.y << "," << std::setw(7) << v.z << ")";
  return out.str();
}

float flat(const glm::vec3& v) { return glm::length(glm::vec2(v.x, v.z)); }

}  // namespace

// The stamp every line carries, so two lines about one frame read together
std::string PlayerWatch::head() const {
  std::ostringstream out;
  out << "[watch] f" << std::setw(6) << frame << " t" << std::fixed << std::setprecision(2)
      << std::setw(8) << clock << "  " << std::left << std::setw(20) << where << std::right;
  return out.str();
}

void PlayerWatch::begin(const std::string& roomName, const glm::vec3& at, float dt) {
  if (!on) return;

  frame++;
  clock += dt;
  sinceBeat += dt;
  where = roomName;
  started = at;
  walked = at;
  step = dt;
}

void PlayerWatch::afterMove(const glm::vec3& at, const glm::vec3& velocity, bool grounded) {
  if (!on) return;

  walked = at;

  // Walking is the only thing allowed to move him sideways, so anything past one
  // step of it came from somewhere else
  const float cap = walkSpeed * step * 1.05f + 0.001f;
  const float went = flat(at - started);
  if (went > cap) {
    std::cout << head() << " WALK  " << vec(at - started) << " flat " << went << " over cap " << cap
              << "  dt " << step << std::endl;
  }

  // A fall the ground should have stopped
  if (!grounded && velocity.y > 1.f) {
    std::cout << head() << " FALL  " << vec(at) << " down " << velocity.y << "/s" << std::endl;
  }

  if (sinceBeat >= heartbeat) {
    sinceBeat = 0.f;
    std::cout << head() << " at    " << vec(at) << " vel " << vec(velocity)
              << (grounded ? "  grounded" : "  in the air") << std::endl;
  }
}

void PlayerWatch::afterSettle(const glm::vec3& at, const std::string& pushedBy) {
  if (!on) return;

  const glm::vec3 shove = at - walked;
  if (glm::length(shove) < shoveShout) return;

  std::cout << head() << " SHOVE " << vec(shove) << " out of "
            << (pushedBy.empty() ? std::string("something with no name") : pushedBy) << "  he was at "
            << vec(walked) << std::endl;
}

void PlayerWatch::placed(const std::string& roomName, const glm::vec3& at, const char* why) {
  if (!on) return;

  where = roomName;
  started = at;
  walked = at;
  std::cout << head() << " PLACED" << vec(at) << " by " << why << std::endl;
}

}  // namespace petscop
