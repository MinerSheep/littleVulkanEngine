#pragma once

#include "petscop/game_state.hpp"
#include "petscop/map_loader.hpp"

#include <string>

namespace petscop {

// Somebody else has been playing your save while the game was shut
//
// He gets one turn for every whole hour you were away. Every turn he walks one
// door, presses something, or stands still, and what comes of it is written into
// the same flags, items and prop memories the game already reads back
//
// All of it is resolved before the first room is built, so none of it happens in
// front of you. playerRoom is the room the save left you in -- he will walk into
// it but he will never change anything in it
//
// Returns how many turns he took
int runOffline(const GameMap& map, GameState& state, int hours, const std::string& playerRoom);

// The four steps of the poem stay yours to finish, so he is kept away from the
// props they run through and from the things you need in your pockets to do them
bool isQuestProp(const std::string& name);
bool isQuestItem(const std::string& name);

}  // namespace petscop
