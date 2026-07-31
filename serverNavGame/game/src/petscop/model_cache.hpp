#pragma once

#include "lve_model.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace petscop {

// Keeps one copy of every mesh a map uses
//
// A preset name is the mesh's file name without the extension, so "cube" means
// models/cube.obj. Asking for the same name twice gives back the same mesh
//
// The cache outlives the room you are standing in, so walking through a door
// loads nothing and throws nothing away
//
// Meshes are handed out as raw pointers and stay good for as long as the cache
// does. Adding a new one never moves the ones already in
class ModelCache {
 public:
  // Load everything a map mentions up front, so no door ever waits on the disk
  void preload(const std::vector<std::string>& presets) {
    for (const std::string& name : presets) get(name);
  }

  // The mesh for a preset, or null if it would not load
  lve::LveModel* get(const std::string& name) {
    auto it = models.find(name);
    if (it != models.end()) return it->second.get();

    const std::string path = "models/" + name + ".obj";
    try {
      std::unique_ptr<lve::LveModel> model = lve::LveModel::createModelFromFile(path);
      lve::LveModel* raw = model.get();
      models.emplace(name, std::move(model));
      return raw;
    } catch (const std::exception& e) {
      std::cerr << "[petscop] could not load preset '" << name << "' from " << path << ": "
                << e.what() << '\n';

      // Remember the failure, so a preset used by fifty objects is only tried once
      models.emplace(name, nullptr);
      return nullptr;
    }
  }

 private:
  std::unordered_map<std::string, std::unique_ptr<lve::LveModel>> models;
};

}  // namespace petscop
