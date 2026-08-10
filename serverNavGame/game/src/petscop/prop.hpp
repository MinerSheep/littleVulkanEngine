#pragma once

#include "collider_component.hpp"
#include "lve_model.hpp"
#include "petscop/map_loader.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace petscop {

// Translate, rotate then scale, the order the editor saves in
glm::mat4 placement(const glm::vec3& t, const glm::vec3& r, const glm::vec3& s);

// How far apart two boxes are, zero once they touch
float boxGap(const ColliderComponent::Aabb& a, const ColliderComponent::Aabb& b);

// Where a prop is on its way to, and how far along it is
struct Motion {
  glm::vec3 fromTranslation{0.f};
  glm::vec3 fromRotation{0.f};
  glm::vec3 fromScale{1.f};
  glm::vec3 toTranslation{0.f};
  glm::vec3 toRotation{0.f};
  glm::vec3 toScale{1.f};

  float elapsed = 0.f;
  float seconds = 0.f;
  bool active = false;
};

// Something solid standing in a room
// The box is held by value, register it once the prop list stops growing
struct Prop {
  lve::LveModel* model = nullptr;
  glm::vec3 translation{0.f};
  glm::vec3 rotation{0.f};
  glm::vec3 scale{1.f};
  ColliderComponent collider{};

  // Which way a wall faces out of the room, all zeroes for anything else
  glm::vec3 face{0.f};

  // Grass and the like never get a box, you walk straight through them
  bool solid = true;

  // 0 hides it, 1 draws it
  float visibility = 1.f;

  // Where the map stood the prop, what a toggle returns to
  glm::vec3 restTranslation{0.f};
  glm::vec3 restRotation{0.f};
  glm::vec3 restScale{1.f};

  std::string name;
  std::vector<MapAction> actions;

  // One per action, saying which way a toggle goes next time
  std::vector<char> flipped;

  Motion motion;
  bool disappeared = false;  // disappeared object cannot interact

  bool interactable() const { return !actions.empty(); }

  glm::mat4 matrix() const;

  // Drags the box back over the mesh after the prop has moved
  void refreshBox();

  // Sends the prop somewhere, or puts it there on the spot when seconds is 0
  void startMotion(const glm::vec3& t, const glm::vec3& r, const glm::vec3& s, float seconds);

  void tickMotion(float dt);
};

// Moves every prop that is on its way somewhere
void tickMotions(std::vector<Prop>& props, float dt);

// Which prop goes by this name, or -1 when nothing does
int findProp(const std::vector<Prop>& props, const std::string& name);

}  // namespace petscop
