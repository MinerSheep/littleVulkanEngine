#pragma once

#include "lve_texture.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

namespace petscop {

// Keeps one copy of every picture a map paints something with
//
// A picture's name is its file name without the extension, out of textures/
// The cache outlives the room, the same way the meshes do
class TextureCache {
 public:
  // The picture by that name, or null when it will not load
  lve::LveTexture* get(const std::string& name) {
    auto it = textures.find(name);
    if (it != textures.end()) return it->second.get();

    const std::string path = "textures/" + name + ".tex";
    try {
      std::unique_ptr<lve::LveTexture> texture = lve::LveTexture::createFromFile(path);
      lve::LveTexture* raw = texture.get();
      textures.emplace(name, std::move(texture));
      return raw;
    } catch (const std::exception& e) {
      std::cerr << "[petscop] could not load texture '" << name << "' from " << path << ": "
                << e.what() << '\n';

      // Remember the failure, a picture ten rooms want is only tried once
      textures.emplace(name, nullptr);
      return nullptr;
    }
  }

 private:
  std::unordered_map<std::string, std::unique_ptr<lve::LveTexture>> textures;
};

}  // namespace petscop
