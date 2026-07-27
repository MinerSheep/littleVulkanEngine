#pragma once

#include "lve_game_object.hpp"  // Component, GameObject, TransformComponent

#include <glm/glm.hpp>

// Simple movement like Unity's Rigidbody
// Holds velocity. Gravity + forces change it.
// Every frame it writes a new position into TransformComponent
//
// Collision is not done here, all this needs is resolveContact()
// to be called in the scene
struct RigidbodyComponent : public Component {
  // How force works (like Unity)
  enum class ForceMode {
    Force,           // slow push, uses mass
    Acceleration,    // slow push, ignores mass
    Impulse,         // instant push, uses mass
    VelocityChange,  // instant push, ignores mass
  };

  // Shared gravity for all bodies
  static glm::vec3 gravity;

  glm::vec3 velocity{0.f};

  float mass = 1.f;
  bool useGravity = true;
  float gravityScale = 1.f;  // per-body gravity strength

  // Kinematic = moved by code, not physics
  // Still blocks others but ignores gravity/forces
  bool isKinematic = false;
  bool enabled = true;

  // Drag slows velocity: v /= (1 + drag * dt)
  float drag = 0.f;

  // Max speed so fast bodies don’t clip through floors
  float maxSpeed = 40.f;

  // Bounce settings.
  // 0 = stop on hit, 1 = full bounce
  // Small hits under bounceThreshold always stop
  float bounciness = 0.f;
  float bounceThreshold = 1.f;

  // Freeze movement on axes (like Unity constraints)
  // Man freezes X/Z so only vertical physics happens
  bool freezePositionX = false;
  bool freezePositionY = false;
  bool freezePositionZ = false;

  // Max time slice per physics step
  // Big frames get split so physics feels same at low FPS
  float maxSubStep = 1.f / 60.f;

  // True when something pushed this body up (jump is allowed when true)
  bool grounded = false;

  // Normal of the surface we stand on
  // While grounded, velocity into this normal is removed
  glm::vec3 contactNormal{0.f, -1.f, 0.f};

  // Add a force - Slow modes stack until update
  // Instant modes change velocity right away
  void addForce(const glm::vec3& force, ForceMode mode = ForceMode::Force);

  // Use the push from collision to fix velocity
  void resolveContact(const glm::vec3& push);

  // Mark body as standing on a surface
  void setGrounded(const glm::vec3& normal);

  void update(float dt, GameObject& obj) override;

 private:
  glm::vec3 pendingAccel{0.f};  // slow forces already mass-adjusted

  void integrate(float h, TransformComponent& transform);
};
