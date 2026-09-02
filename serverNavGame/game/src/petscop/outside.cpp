#include "petscop/outside.hpp"

#include <cstdlib>
#include <ctime>

namespace petscop {

namespace {

// Where the four bands of the day start
const float dawnAt = 5.f;
const float dayAt = 7.f;
const float duskAt = 18.f;
const float nightAt = 20.f;

// What the house looks like at either end of the day
const float nightGain = 0.55f;
const float dayGain = 1.4f;
const float dayAmbient = 1.6f;
const float dayBackdrop = 0.4f;

// Seconds between anything happening at midday, and none of it after dark
const float dayHoldOff = 25.f;

// How high the sun has to be before the quest props are gone, about half six
const float questsGoneAbove = 0.75f;

const glm::vec3 nightWash{0.74f, 0.82f, 1.f};
const glm::vec3 dayWash{1.f, 0.96f, 0.86f};

// How high the sun is, ramped through dawn and dusk rather than switched
float sunAt(float hour) {
  if (hour < dawnAt || hour >= nightAt) return 0.f;
  if (hour < dayAt) return (hour - dawnAt) / (dayAt - dawnAt);
  if (hour < duskAt) return 1.f;
  return 1.f - (hour - duskAt) / (nightAt - duskAt);
}

float ramp(float from, float to, float t) { return from + (to - from) * t; }

}  // namespace

long long nowSeconds() { return static_cast<long long>(std::time(nullptr)); }

int hoursBetween(long long from, long long to, int cap) {
  if (from <= 0 || to <= from) return 0;

  const long long hours = (to - from) / 3600;
  if (hours <= 0) return 0;
  if (cap > 0 && hours > cap) return cap;
  return static_cast<int>(hours);
}

float hourOfDay() {
  // PETSCOP_HOUR=14 stands the game in the afternoon whatever the clock says
  if (const char* forced = std::getenv("PETSCOP_HOUR")) return static_cast<float>(std::atof(forced));

  const std::time_t stamp = static_cast<std::time_t>(nowSeconds());
  const std::tm* local = std::localtime(&stamp);

  // A clock that will not read is two in the morning, the answer that keeps the
  // game playable rather than the one that hides the cue
  if (!local) return 2.f;

  return static_cast<float>(local->tm_hour) + static_cast<float>(local->tm_min) / 60.f +
         static_cast<float>(local->tm_sec) / 3600.f;
}

Daylight daylightAt(float hour) {
  Daylight sky;
  sky.sun = sunAt(hour);
  sky.gain = ramp(nightGain, dayGain, sky.sun);
  sky.ambient = ramp(1.f, dayAmbient, sky.sun);
  sky.wash = nightWash + (dayWash - nightWash) * sky.sun;
  sky.backdrop = ramp(1.f, dayBackdrop, sky.sun);
  sky.holdOff = dayHoldOff * sky.sun;
  sky.hidesQuests = sky.sun > questsGoneAbove;
  return sky;
}

Daylight daylightNow() { return daylightAt(hourOfDay()); }

}  // namespace petscop
