#pragma once

#include "petscop/game_state.hpp"

#include <string>

namespace petscop {

// Reads a save back into state, wiping whatever was there first
// Returns false when there is no file yet, which is a new game rather than a fault
bool readSave(const std::string& path, GameState& state);

// Writes the save out, making the folder it sits in if it is missing
bool writeSave(const std::string& path, const GameState& state);

// Where saves should live outside the project folder: %APPDATA%/petscop/saves
// on Windows, $XDG_DATA_HOME/petscop/saves (or ~/.local/share/petscop/saves)
// on Linux. Falls back to the current directory if no home/appdata can be
// found at all, which should only happen in a very stripped down environment
std::string defaultSaveDirectory();

// defaultSaveDirectory() plus one file name, e.g. defaultSavePath("forest.save")
std::string defaultSavePath(const std::string& fileName);

}  // namespace petscop
