#include "collision_system.hpp"

void CollisionSystem::addStatic(const ColliderComponent* collider) {
  if (collider) statics.push_back(collider);
}

bool CollisionSystem::addDynamic(GameObject& obj, ColliderComponent* collider,
                                 RigidbodyComponent* body) {
  if (!collider) return false;

  TransformComponent* xform = obj.getComponent<TransformComponent>();
  if (!xform) return false;  // nothing to move, so nothing to settle

  bodies.push_back({xform, collider, body});
  return true;
}

void CollisionSystem::clear() {
  statics.clear();
  bodies.clear();
}

bool CollisionSystem::pushApart(TransformComponent& xform, ColliderComponent& collider,
                                RigidbodyComponent* body, const ColliderComponent& solid) {
  if (!solid.enabled) return false;
  if (&solid == &collider) return false;  // never test a body against itself

  const glm::vec3 push = collider.worldBox().pushOut(solid.worldBox());
  if (push == glm::vec3(0.f)) return false;

  // The hardest shove of the settle is kept so a watcher can name what did it
  if (glm::dot(push, push) > glm::dot(lastPush, lastPush)) {
    lastPush = push;
    lastPusher = &solid;
  }

  xform.translation += push;

  // The box has to follow the move right away, or the next test this pass reads
  // where the body used to be
  collider.refresh(xform.mat4());

  // Let the rigidbody cancel or bounce the velocity that drove it in here
  if (body) body->resolveContact(push);
  return true;
}

bool CollisionSystem::isSupported(const ColliderComponent& collider) const {
  // Probe a thin slab just past the underside of the box
  // +Y, because -Y is up
  ColliderComponent::Aabb feet = collider.worldBox();
  feet.min.y = feet.max.y;
  feet.max.y += groundProbeDepth;

  auto standingOn = [&](const ColliderComponent& solid) {
    return solid.enabled && &solid != &collider && feet.overlaps(solid.worldBox());
  };

  for (const ColliderComponent* solid : statics) {
    if (solid && standingOn(*solid)) return true;
  }
  for (const Body& other : bodies) {
    if (other.collider && standingOn(*other.collider)) return true;
  }
  return false;
}

void CollisionSystem::settle(TransformComponent& xform, ColliderComponent& collider,
                             RigidbodyComponent* body) {
  lastPush = glm::vec3(0.f);
  lastPusher = nullptr;
  if (!collider.enabled) return;

  for (int pass = 0; pass < passes; pass++) {
    bool pushed = false;

    // |= not ||, so every collider is tested and not just up to the first hit
    for (const ColliderComponent* solid : statics) {
      if (solid) pushed |= pushApart(xform, collider, body, *solid);
    }

    // Only the body being settled moves here
    // Whatever it landed on stays put and settles on its own turn
    for (const Body& other : bodies) {
      if (other.collider) pushed |= pushApart(xform, collider, body, *other.collider);
    }

    if (!pushed) break;  // nothing left to resolve
  }

  // This is what tells the man he is standing on something and may jump
  if (body && isSupported(collider)) body->setGrounded(groundNormal);
}

void CollisionSystem::settleAll() {
  for (const Body& b : bodies) {
    if (b.xform && b.collider) settle(*b.xform, *b.collider, b.body);
  }
}
