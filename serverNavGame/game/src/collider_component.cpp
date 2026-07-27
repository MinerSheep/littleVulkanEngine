#include "collider_component.hpp"

// std
#include <cmath>
#include <limits>

// ColliderComponent computes a local bound based off the object center
// It then matches it to the model and current transform

bool ColliderComponent::Aabb::overlaps(const Aabb& other) const {
  // one box's min must be less than the other's max
  // and vice versa
  // checks all axes
  return min.x <= other.max.x && max.x >= other.min.x &&
         min.y <= other.max.y && max.y >= other.min.y &&
         min.z <= other.max.z && max.z >= other.min.z;
}

// Computes penetration depth to send a moving object back however far it went in
glm::vec3 ColliderComponent::Aabb::pushOutXZ(const Aabb& other) const {
  if (!overlaps(other)) return glm::vec3(0.f);

  // How far this box has sunk into the other along each horizontal direction
  const float outPosX = other.max.x - min.x;  // shove toward +X
  const float outNegX = max.x - other.min.x;  // shove toward -X
  const float outPosZ = other.max.z - min.z;  // shove toward +Z
  const float outNegZ = max.z - other.min.z;
  
  // The shallowest of the four is the cheapest way back out
  const float dx = (outPosX < outNegX) ? outPosX : -outNegX;
  const float dz = (outPosZ < outNegZ) ? outPosZ : -outNegZ;

  // Boxes that only touch overlap by exactly 0, so this falls out as no push
  return (std::abs(dx) < std::abs(dz)) ? glm::vec3(dx, 0.f, 0.f) : glm::vec3(0.f, 0.f, dz);
}

// Same idea as pushOutXZ but the vertical axis competes too, so a body can land
// on top of something instead of only ever sliding around it
glm::vec3 ColliderComponent::Aabb::pushOut(const Aabb& other) const {
  if (!overlaps(other)) return glm::vec3(0.f);

  const float outPosX = other.max.x - min.x;
  const float outNegX = max.x - other.min.x;
  const float outPosY = other.max.y - min.y;
  const float outNegY = max.y - other.min.y;
  const float outPosZ = other.max.z - min.z;
  const float outNegZ = max.z - other.min.z;

  const float dx = (outPosX < outNegX) ? outPosX : -outNegX;
  const float dy = (outPosY < outNegY) ? outPosY : -outNegY;
  const float dz = (outPosZ < outNegZ) ? outPosZ : -outNegZ;

  // Whichever axis it has sunk into least is the cheapest way back out
  if (std::abs(dx) < std::abs(dy) && std::abs(dx) < std::abs(dz)) return glm::vec3(dx, 0.f, 0.f);
  if (std::abs(dy) < std::abs(dz)) return glm::vec3(0.f, dy, 0.f);
  return glm::vec3(0.f, 0.f, dz);
}

ColliderComponent::Aabb ColliderComponent::Aabb::expanded(float amount) const {
  return Aabb{min - glm::vec3(amount), max + glm::vec3(amount)};
}

void ColliderComponent::fitToModel(const lve::LveModel& model) {
  setLocalBox(0.5f * (model.boundsMin() + model.boundsMax()),
              0.5f * (model.boundsMax() - model.boundsMin()));
}

void ColliderComponent::fitToModel(const lve::LveSkinnedModel& model) {
  setLocalBox(0.5f * (model.boundsMin() + model.boundsMax()),
              0.5f * (model.boundsMax() - model.boundsMin()));
}

void ColliderComponent::setLocalBox(const glm::vec3& center, const glm::vec3& halfExtent) {

  localCenter = center;

  // A quad (or any flat mesh) measures zero thickness on one axis
  // It is given a sliver instead - to avoid a knife edge
  localHalfExtent = glm::max(glm::abs(halfExtent), glm::vec3(0.0001f));
}

// transform local AABB into world AABB w transform matrix
void ColliderComponent::refresh(const glm::mat4& modelMatrix) {
  glm::vec3 lo(std::numeric_limits<float>::max());
  glm::vec3 hi(std::numeric_limits<float>::lowest());

  // Push all 8 corners of the local box through the transform and wrap a new
  // axis-aligned box around the results

  // This uses bit masking where 0 is x, 1 is y, 2 is z
  for (int corner = 0; corner < 8; corner++) {
    const glm::vec3 local{
        localCenter.x + ((corner & 1) ? localHalfExtent.x : -localHalfExtent.x),
        localCenter.y + ((corner & 2) ? localHalfExtent.y : -localHalfExtent.y),
        localCenter.z + ((corner & 4) ? localHalfExtent.z : -localHalfExtent.z)};

    // using modelMatrix applies also rotation & scale
    const glm::vec3 world3 = glm::vec3(modelMatrix * glm::vec4(local, 1.f));
    lo = glm::min(lo, world3);
    hi = glm::max(hi, world3);
  }

  // set bounds
  world.min = lo;
  world.max = hi;
}

void ColliderComponent::update(float dt, GameObject& obj) {
  if (!enabled) return;

  TransformComponent* transform = obj.getComponent<TransformComponent>();
  if (!transform) return;  // nothing to place the box with

  refresh(transform->mat4());
}
