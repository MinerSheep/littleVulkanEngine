#pragma once

namespace petscop {

// What the machine outside the game knows
//
// One place in the codebase reads the clock. X07 asks it how long he was gone and
// X11 will ask it what time of day it is

// Seconds since the epoch, off the machine's clock
long long nowSeconds();

// Whole hours between two stamps, which is how many turns he gets
//
// Under an hour is none, so relaunching over and over moves nothing. A gap longer
// than the cap is worth the cap, so a fortnight away does not finish the game.
// A clock that has gone backwards is worth none at all
int hoursBetween(long long from, long long to, int cap = 12);

}  // namespace petscop
