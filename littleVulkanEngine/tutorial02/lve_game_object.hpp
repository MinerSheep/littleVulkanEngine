#pragma once

#include "lve_model.hpp"

// libs - helps with mat4
#include <glm/gtc/matrix_transform.hpp>

// std
#include <memory>

namespace lve {

struct TransformComponent {
  glm::vec3 translation{};  // (position offset)
  glm::vec3 scale{1.f, 1.f, 1.f};
  glm::vec3 rotation;

  // This returns rotation & scale data formatted into a matrix
  glm::mat4 mat4()
  {
    auto transform = glm::translate(glm::mat4{1.f}, translation);

    // y z x tait-bryan angle
    // Matrix corresponds to translate * Ry * Rx * Rz * scale transformation
    transform = glm::rotate(transform, rotation.y, {0.f,1.f,0.f});
    transform = glm::rotate(transform, rotation.x, {1.f,0.f,0.f});
    transform = glm::rotate(transform, rotation.z, {0.f,0.f,1.f});


    transform = glm::scale(transform, scale);
    return transform;
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
  TransformComponent transform{};

 private:
  LveGameObject(id_t objId) : id{objId} {}

  id_t id;
};
}  // namespace lve