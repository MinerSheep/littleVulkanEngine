#include "rigidbody_component.hpp"

// std
#include <algorithm>
#include <cmath>

// -Y is up here, so falling is +Y
glm::vec3 RigidbodyComponent::gravity{0.f, 9.81f, 0.f};

void RigidbodyComponent::addForce(const glm::vec3& force, ForceMode mode) {
  if (isKinematic || !enabled) return;

  // Min mass is 1e-6f to prevent division by 0
  const float m = (mass > 1e-6f) ? mass : 1e-6f;

  switch (mode) {
    case ForceMode::Force:          pendingAccel += force / m; break;
    case ForceMode::Acceleration:   pendingAccel += force;     break;
    case ForceMode::Impulse:        velocity += force / m;     break;
    case ForceMode::VelocityChange: velocity += force;         break;
  }
}


// We are standing on a surface
void RigidbodyComponent::setGrounded(const glm::vec3& normal) {
  if (isKinematic || !enabled) return;
  if (glm::dot(normal, normal) < 1e-12f) return;

  grounded = true;

  // contactNormal blocks velocity in that dir
  contactNormal = glm::normalize(normal);
}

// Handle collision response after collision component runs
void RigidbodyComponent::resolveContact(const glm::vec3& push) {
  if (isKinematic || !enabled) return;
  if (glm::dot(push, push) < 1e-12f) return;

  const glm::vec3 normal = glm::normalize(push);

  // The -0.5 keeps a glancing shove off a wall from counting as a floor
  if (normal.y < -0.5f) setGrounded(normal);

  // Approach speed - how fast moving into surface
  const float approach = glm::dot(velocity, normal);

  // Dont add additional energy if moving away
  if (approach >= 0.f) return;

  if (bounciness > 0.f && -approach > bounceThreshold) {
    // Reflect: remove the approach component and add it back the other way
    // (scale by how lively the material is)
    velocity -= (1.f + bounciness) * approach * normal;
  } else {
    // Cancel only the component into the surface, which leaves room to slide along surface
    velocity -= approach * normal;
  }
}

void RigidbodyComponent::integrate(float h, TransformComponent& transform) {

  // acceleration
  glm::vec3 accel = pendingAccel;
  if (useGravity) accel += gravity * gravityScale;

  // velocity
  velocity += accel * h;

  // drag
  if (drag > 0.f) velocity /= (1.f + drag * h);

  if (grounded) {
    // remove any velocity going into the ground
    const float into = glm::dot(velocity, contactNormal);
    if (into < 0.f) velocity -= into * contactNormal;
  }

  // check for frozen axes
  if (freezePositionX) velocity.x = 0.f;
  if (freezePositionY) velocity.y = 0.f;
  if (freezePositionZ) velocity.z = 0.f;

  // terminal velocity
  const float speed = glm::length(velocity);
  if (speed > maxSpeed && speed > 0.f) velocity *= maxSpeed / speed;

  // translate
  transform.translation += velocity * h;
}

void RigidbodyComponent::update(float dt, GameObject& obj) {
  if (!enabled || dt <= 0.f) return;

  if (isKinematic) {
    // Hold it still so it does not lurch the moment this is
    // handed back to physics
    velocity = glm::vec3(0.f);
    pendingAccel = glm::vec3(0.f);
    return;
  }

  TransformComponent* transform = obj.getComponent<TransformComponent>();
  if (!transform) return;  // nothing to move

  // Split the frame into slices no longer than maxSubStep so behaviour does not
  // change with the frame rate.  ceil, so the slices are never longer than asked
  const int steps = std::max(1, static_cast<int>(std::ceil(dt / maxSubStep)));
  const float h = dt / static_cast<float>(steps);

  for (int i = 0; i < steps; i++) integrate(h, *transform);

  pendingAccel = glm::vec3(0.f);  // forces one last frame

  // The contact pass runs after every component, so clearing this
  // here leaves it to be re-raised for whatever the body is touching NOW
  
  // What integrate() read above was last frame's answer, which is what it wants:
  // a body that was resting is still resting until told otherwise
  grounded = false;
}
