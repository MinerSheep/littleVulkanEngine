#pragma once

#include "lve_game_object.hpp"    // Component, GameObject, TransformComponent
#include "lve_model.hpp"          // lve::LveModel (mesh bounds)
#include "lve_skinned_model.hpp"  // lve::LveSkinnedModel (bind-pose bounds)

#include <glm/glm.hpp>

// Box collision for anything that has a placement
//
// The box is kept in model space (center + half extents, normally sized straight
// off the mesh by fitToModel) and re-projected into a world-space Axis Aligned
// Bounding Box whenever the owner moves

// Rotation is handled by transforming the 8 corners and fitting
// a fresh axis-aligned box around them, so a spun prop still blocks - just with
// a box that is a little generous on the diagonals
//
// A GameObject picks one up through addComponent and update() keeps its world box
// in step with the sibling TransformComponent. Anything that is not a GameObject
// (SkinnedDemoScene's StaticProp) can hold one directly and call refresh() itself
struct ColliderComponent : public Component {

  // World space axis-aligned box, use refresh() to rebuild
  struct Aabb {
    glm::vec3 min{0.f};
    glm::vec3 max{0.f};

    glm::vec3 center() const { return 0.5f * (min + max); }
    glm::vec3 halfExtent() const { return 0.5f * (max - min); }

    bool overlaps(const Aabb& other) const;

    // Shortest horizontal shove that separates this box from other
    // Y is left alone: these props all stand on one ground plane
    glm::vec3 pushOutXZ(const Aabb& other) const;



    // --- Interaction with RigidBody ------------------------------------------

    // Shortest shove along ANY axis, vertical included (unlike pushOutXZ)
    //
    // Being the shortest way out, it lifts a body onto a low prop once it has
    // sunk further into the sides than into the top: a short block is steppable,
    // a tall one is still a wall
    glm::vec3 pushOut(const Aabb& other) const;

    // A copy that is grown by `amount` on every axis, for probing just past a surface
    // 
    // A body resting won't detect floor contact (overlaps it by zero)
    // We have settleBody in skinneddemoscene for this purpose
    Aabb expanded(float amount) const;
  };

  // Model space box, basically before the owner's translate/rotate/scale
  glm::vec3 localCenter{0.f};
  glm::vec3 localHalfExtent{0.5f};

  bool isStatic = true; // Static colliders block others but never get pushed
  bool enabled = true;

  // Size the local box to a mesh's own bounds
  // Measures the bind pose, ignores animation
  void fitToModel(const lve::LveModel& model);
  void fitToModel(const lve::LveSkinnedModel& model);

  // Set the local box by hand, e.g. to keep a character's footprint narrower than
  // their mesh. Half extents are clamped away from zero so a flat mesh still
  // reports a real overlap rather than a plane that everything slips past
  void setLocalBox(const glm::vec3& center, const glm::vec3& halfExtent);

  // Re-project the local box through a world matrix -- the same TRS the object is
  // drawn with -- and cache the result in worldBox()
  void refresh(const glm::mat4& modelMatrix);
  const Aabb& worldBox() const { return world; }

  // Keeps the world box in step with the sibling TransformComponent. Add the
  // collider AFTER whatever component moves the object, so components run in an
  // order that leaves the box reflecting this frame's move, not the last one
  void update(float dt, GameObject& obj) override;

 private:
  Aabb world{};
};
