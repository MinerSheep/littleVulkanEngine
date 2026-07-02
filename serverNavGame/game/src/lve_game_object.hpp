#pragma once

#include "lve_model.hpp"

// libs - helps with mat4
#include <glm/gtc/matrix_transform.hpp>

// std
#include <memory>
#include <unordered_map>

struct TransformComponent {
  glm::vec3 translation{};  // (position offset)
  glm::vec3 scale{1.f, 1.f, 1.f};
  glm::vec3 rotation;

  // This returns rotation & scale data formatted into a matrix
  glm::mat4 mat4();
  glm::mat3 normalMatrix();
};

// pairs with transform component for location
struct PointLightComponent
{
  float lightIntensity = 1.0f;
};

class GameObject {
 public:
  using id_t = unsigned int;
  using Map = std::unordered_map<id_t, GameObject>;

  static GameObject createGameObject() {
    static id_t currentId = 0;
    return GameObject{currentId++};
  }

  static GameObject makePointLight(float intensity = 1.0f, float radius = 0.1f, glm::vec3 color = glm::vec3(1.f));

  // COPY - DELETE
  GameObject(const GameObject&) = delete;
  GameObject& operator=(const GameObject&) = delete;

  // MOVE
  GameObject(GameObject&&) = default;
  GameObject& operator=(GameObject&&) = default;

  id_t getId() { return id; }

  glm::vec3 color{};
  TransformComponent transform{};
  
  // Optional: has a model shape,   color,   transform
  std::shared_ptr<lve::LveModel> model{};
  std::unique_ptr<PointLightComponent> pointLight = nullptr;

 private:
  GameObject(id_t objId) : id{objId} {}

  id_t id;
};