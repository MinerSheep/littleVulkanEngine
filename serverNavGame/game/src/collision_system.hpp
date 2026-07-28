#pragma once

#include "collider_component.hpp"
#include "lve_game_object.hpp"
#include "rigidbody_component.hpp"

#include <glm/glm.hpp>
#include <vector>

// Pushes bodies back out of anything solid they have moved into
//
// ColliderComponent knows the shape of a box and RigidbodyComponent knows how to
// fall, but neither one resolves a hit. This is the piece that does, so a scene
// gets physics by registering its colliders instead of writing its own resolver
//
// How a scene uses it:
//   1. build all the colliders, then register them once (see the warning below)
//   2. every frame, update the components first so everything has moved
//   3. call settleAll() before the camera and the draw list read any positions
//
// WARNING: the system stores pointers, it does not own anything
// Register only AFTER the vectors holding your props and bodies have stopped
// growing, because a push_back moves their contents and leaves the old pointers dangling
class CollisionSystem {
 public:
  // Something solid that never moves, like the ground or a prop
  // Static colliders block bodies but are never pushed themselves
  void addStatic(const ColliderComponent* collider);

  // Something that moves and gets pushed out, like the player or a falling box
  // body may be null for a mover with no physics, it just gets shoved out of walls
  //
  // Returns false and registers nothing when the object has no TransformComponent,
  // since there would be nothing to move
  bool addDynamic(GameObject& obj, ColliderComponent* collider, RigidbodyComponent* body);

  // Forget every registered collider, for a scene that rebuilds its world
  void clear();

  // Push every registered body out of what it moved into
  void settleAll();

  // Same for one body that is not registered, tested against everything that is
  void settle(TransformComponent& xform, ColliderComponent& collider, RigidbodyComponent* body);

  // How many times to re-check after a shove
  // The first shove can slide a body straight into a neighbour, so one pass is not enough
  // Anything still overlapping after the last pass is wedged, and stopping beats
  // jittering between two props
  int passes = 2;

  // How far past a body's underside to look for a surface holding it up
  // A body resting on the floor sinks into it by exactly zero, so the passes above
  // find nothing and would never call it grounded
  float groundProbeDepth = 0.03f;

  // Which way is up, for the grounded flag
  // -Y is up in this engine
  glm::vec3 groundNormal{0.f, -1.f, 0.f};

 private:
  struct Body {
    TransformComponent* xform = nullptr;
    ColliderComponent* collider = nullptr;
    RigidbodyComponent* body = nullptr;
  };

  std::vector<const ColliderComponent*> statics;
  std::vector<Body> bodies;

  // Push one box out of one other box
  // Returns true when it actually moved, so a pass knows to run again
  bool pushApart(TransformComponent& xform, ColliderComponent& collider,
                 RigidbodyComponent* body, const ColliderComponent& solid);

  // Is anything holding this box up?
  bool isSupported(const ColliderComponent& collider) const;
};
