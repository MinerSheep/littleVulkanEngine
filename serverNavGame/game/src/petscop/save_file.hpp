#pragma once

#include "petscop/game_state.hpp"

#include <string>

namespace petscop {

// Reads a save back into state, wiping whatever was there first
// Returns false when there is no file yet, which is a new game rather than a fault
bool readSave(const std::string& path, GameState& state);

// Writes the save out, making the folder it sits in if it is missing
bool writeSave(const std::string& path, const GameState& state);

}  // namespace petscop
