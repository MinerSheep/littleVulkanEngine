#pragma once

#include "lve_model.hpp"

// libs - helps with mat4
#include <glm/gtc/matrix_transform.hpp>

// std
#include <memory>
#include <unordered_map>

struct Component {
    virtual ~Component() = default;
    virtual void update(float dt, class GameObject& obj) {}
};

struct TransformComponent : public Component {
  glm::vec3 translation{};  // (position offset)
  glm::vec3 scale{1.f, 1.f, 1.f};
  glm::vec3 rotation{};

  // This returns rotation & scale data formatted into a matrix
  glm::mat4 mat4();
  glm::mat3 normalMatrix();

  // glm::mat2 mat2() {
  //   float s = glm::sin(rotation.x);
  //   float c = glm::cos(rotation.x);
  //   glm::mat2 rotMatrix{{c, s}, {-s, c}};

  //   glm::mat2 scaleMat{{scale.x, .0f}, {.0f, scale.y}};
  //   return rotMatrix * scaleMat;
  // }
};

// pairs with transform component for location
struct PointLightComponent : public Component
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
  
  // Optional: has a model shape,   color,   transform
  // bool UI = false;
  std::shared_ptr<lve::LveModel> model{};

  template <typename T, typename... Args>
  T* addComponent(Args&&... args) {
      // static_assert(std::is_base_of_v<Component, T>::value,
      //               "T must inherit from Component");

      auto comp = std::make_unique<T>(std::forward<Args>(args)...);
      T* ptr = comp.get();
      components.emplace_back(std::move(comp));
      return ptr;
  }

  template <typename T>
  T* getComponent() {
      // static_assert(std::is_base_of_v<Component, T>::value,
      //               "T must inherit from Component");

      for (auto& c : components) {
          if (auto ptr = dynamic_cast<T*>(c.get())) {
              return ptr;
          }
      }
      return nullptr;
  }


  void updateComponents(float dt) {
    for (auto& c : components) {
        c->update(dt, *this);
    }
}

 private:
  GameObject(id_t objId) : id{objId} {}

  id_t id;
  std::vector<std::unique_ptr<Component>> components;
};