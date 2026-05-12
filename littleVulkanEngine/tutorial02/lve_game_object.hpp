#pragma once

#include "lve_model.hpp"

// std
#include <memory>

namespace lve {

struct Transform2dComponent {
  glm::vec2 translation{};  // (position offset)
  glm::vec2 scale{1.f, 1.f};
  float rotation;

  // This returns rotation & scale data formatted into a matrix
  glm::mat2 mat2() {
    const float s = glm::sin(rotation);
    const float c = glm::cos(rotation);
    glm::mat2 rotMatrix{{c, s}, {-s, c}};  // rotation matrix
    /*
    [cos 0  -sin 0]
    [sin 0   cos 0]
    */

    // GLM MATRIX CONSTRUCTOR constructs by COLUMNS
    glm::mat2 scaleMat{{scale.x, .0f}, {.0f, scale.y}};
    // scale * rot = 3d effect
    // rot * scale = 2d effect
    return rotMatrix * scaleMat;
  }
};

class LveGameObject {
 public:
  using id_t = unsigned int;

  static LveGameObject createGameObject() {
    static id_t currentId = 0;
    return LveGameObject{currentId++};
  }

  // COPY - DELETE
  LveGameObject(const LveGameObject &) = delete;
  LveGameObject &operator=(const LveGameObject &) = delete;

  // MOVE
  LveGameObject(LveGameObject &&) = default;
  LveGameObject &operator=(LveGameObject &&) = default;

  id_t getId() { return id; }

  // has a model shape,   color,   transform
  std::shared_ptr<LveModel> model{};
  glm::vec3 color{};
  Transform2dComponent transform2d{};

 private:
  LveGameObject(id_t objId) : id{objId} {}

  id_t id;
};
}  // namespace lve