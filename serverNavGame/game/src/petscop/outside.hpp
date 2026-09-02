#pragma once

#include <glm/glm.hpp>

namespace petscop {

// What the machine outside the game knows
//
// One place in the codebase reads the clock. X07 asks it how long he was gone and
// X11 asks it what time of day it is

// Seconds since the epoch, off the machine's clock
long long nowSeconds();

// Whole hours between two stamps, which is how many turns he gets
//
// Under an hour is none, so relaunching over and over moves nothing. A gap longer
// than the cap is worth the cap, so a fortnight away does not finish the game.
// A clock that has gone backwards is worth none at all
int hoursBetween(long long from, long long to, int cap = 12);

// The hour on the machine's own clock, 0 to 24 with the minutes in the fraction
// A clock that will not read comes back as the middle of the night
float hourOfDay();

// What the sun is doing to the house
struct Daylight {
  float sun = 0.f;       // 0 after dark, 1 at midday, ramped through dawn and dusk
  float gain = 1.f;      // every light in the room is scaled by this
  float ambient = 1.f;   // and so is the wash sitting over everything
  glm::vec3 wash{1.f};   // warm at noon, cold once it is dark
  float backdrop = 1.f;  // how fast the bars behind the room march
  float holdOff = 0.f;   // seconds the house waits before doing anything else

  // The cue is not on the table and the spade is not in the shed
  bool hidesQuests = false;
};

Daylight daylightAt(float hour);
Daylight daylightNow();

}  // namespace petscop
