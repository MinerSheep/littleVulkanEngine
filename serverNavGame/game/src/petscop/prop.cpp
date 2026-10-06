#include "petscop/prop.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace petscop {

glm::mat4 placement(const glm::vec3& t, const glm::vec3& r, const glm::vec3& s) {
  glm::mat4 mat = glm::translate(glm::mat4(1.f), t);
  mat = glm::rotate(mat, r.y, glm::vec3(0.f, 1.f, 0.f));
  mat = glm::rotate(mat, r.x, glm::vec3(1.f, 0.f, 0.f));
  mat = glm::rotate(mat, r.z, glm::vec3(0.f, 0.f, 1.f));
  return glm::scale(mat, s);
}

float boxGap(const ColliderComponent::Aabb& a, const ColliderComponent::Aabb& b) {
  const glm::vec3 gap = glm::max(glm::max(a.min - b.max, b.min - a.max), glm::vec3(0.f));
  return glm::length(gap);
}

glm::mat4 Prop::matrix() const { return placement(translation, rotation, scale); }

void Prop::refreshBox() { collider.refresh(matrix()); }

void Prop::startMotion(const glm::vec3& t, const glm::vec3& r, const glm::vec3& s, float seconds) {
  if (seconds <= 0.f) {
    translation = t;
    rotation = r;
    scale = s;
    motion.active = false;
    refreshBox();
    return;
  }

  motion.fromTranslation = translation;
  motion.fromRotation = rotation;
  motion.fromScale = scale;
  motion.toTranslation = t;
  motion.toRotation = r;
  motion.toScale = s;
  motion.elapsed = 0.f;
  motion.seconds = seconds;
  motion.active = true;
}

void Prop::tickMotion(float dt) {
  if (!motion.active) return;

  motion.elapsed += dt;

  float amount = motion.elapsed / motion.seconds;
  if (amount >= 1.f) {
    amount = 1.f;
    motion.active = false;
  }

  // Smoothstep, eases in and out of the move
  const float eased = amount * amount * (3.f - 2.f * amount);
  translation = glm::mix(motion.fromTranslation, motion.toTranslation, eased);
  rotation = glm::mix(motion.fromRotation, motion.toRotation, eased);
  scale = glm::mix(motion.fromScale, motion.toScale, eased);
  refreshBox();
}

void tickMotions(std::vector<Prop>& props, float dt) {
  for (Prop& prop : props) prop.tickMotion(dt);
}

int findProp(const std::vector<Prop>& props, const std::string& name) {
  if (name.empty()) return -1;
  for (std::size_t i = 0; i < props.size(); i++) {
    if (props[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace petscop
