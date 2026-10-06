#include "player_ability_component.hpp"

#include "lve_engine.hpp"

// std
#include <algorithm>
#include <cmath>

void PlayerAbilityComponent::update(float dt, GameObject& obj) {
  GLFWwindow* window = lve::LveEngine::instance().getGLFWWindow();

  // One projectile per key-down
  bool down = glfwGetKey(window, fireKey) == GLFW_PRESS;

  if (down && !firePrevDown) fire(obj);
  firePrevDown = down;

  // Advance live projectiles, tracking distance flown so we can cull them
  for (auto& p : projectiles) {
    glm::vec3 step = p.velocity * dt;
    p.position += step;
    p.distanceTravelled += glm::length(step);

    // Then ask the scene whether that landed the shot in something solid.
    // One test per frame is enough while a step stays shorter than the ball is
    // wide (speed * dt < radius * 2), because consecutive positions then overlap
    // and leave no gap for a thin prop to slip through
    if (onImpact && onImpact(p)) p.spent = true;
  }

  // Deleting spent and old fireballs
  projectiles.erase(
      std::remove_if(
          projectiles.begin(), projectiles.end(),
          [&](const Projectile& p) { return p.spent || p.distanceTravelled >= maxRange; }),
      projectiles.end());
}

void PlayerAbilityComponent::fire(GameObject& obj) {
  TransformComponent* t = obj.getComponent<TransformComponent>();
  if (!t) return;

  // Forward from aimYaw, same {sin,0,cos} convention as KeyboardMovementComponent
  const glm::vec3 forward{std::sin(aimYaw), 0.f, std::cos(aimYaw)};

  Projectile p;
  p.position = t->translation + muzzleOffset + forward * muzzleForward;
  p.velocity = forward * speed;
  projectiles.push_back(p);
}
