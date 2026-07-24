#pragma once

#include "lve_game_object.hpp"  // Component, GameObject, TransformComponent
#include "lve_window.hpp"       // GLFW key codes + GLFWwindow
#include "lve_model.hpp"

#include <glm/glm.hpp>

// std
#include <memory>
#include <vector>

// A player ability: press a button to launch a small sphere ("fireball") forward a
// short distance along an aim direction (from the camera)
//
// requires the scene to draw the fireballs, all update logic contained here
struct PlayerAbilityComponent : public Component {
  struct Projectile {
    glm::vec3 position{0.f};
    glm::vec3 velocity{0.f};
    float distanceTravelled = 0.f;
  };

  int fireKey = GLFW_KEY_F;

  // The scene sets this each frame, KeyboardMovementComponent::forwardYaw
  float aimYaw = 0.f;

  float speed = 9.f;      // units per second
  float maxRange = 6.f;   // the "short distance": culled after travelling this far
  float radius = 0.2f;    // sphere scale

  // Spawn offset from the owner's translation
  glm::vec3 muzzleOffset{0.f, -0.8f, 0.f};
  // Forward is how far forward the ball initially spawns
  float muzzleForward = 0.5f;

  void setModel(std::unique_ptr<lve::LveModel> m) { projectileModel = std::move(m); }
  lve::LveModel* model() const { return projectileModel.get(); }
  const std::vector<Projectile>& activeProjectiles() const { return projectiles; }

  void update(float dt, GameObject& obj) override;

 private:
  std::unique_ptr<lve::LveModel> projectileModel;   // owned
  std::vector<Projectile> projectiles;              // live shots
  bool firePrevDown = false;                        // for key-down edge detection

  void fire(GameObject& obj);
};
