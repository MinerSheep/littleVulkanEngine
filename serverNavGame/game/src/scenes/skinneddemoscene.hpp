#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "lve_model.hpp"
#include "skinned_model_component.hpp"
#include "keyboard_movement_component.hpp"
#include "player_ability_component.hpp"
#include "collider_component.hpp"
#include "rigidbody_component.hpp"

#include <glm/gtc/constants.hpp>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Minimal scene that shows a single skinned (skeletal) glTF mesh in isolation, so
// the new skinning path can be seen without the clutter of the other scenes

// WASD walks the man around the XZ plane (relative to the camera); hold the left
// mouse button and drag to orbit the camera around him
class SkinnedDemoScene : public lve::LveScene {
 public:
  SkinnedDemoScene() {}
  void update(float dt) override;
  void onEvent(const lve::Event& event) override;
  void cleanup() override;

  void loadModels() override;
  void setupLights() override;

 private:
  lve::LveCamera camera{};

  // Orbit-follow camera: the mouse spins it around the man. Camera position is the
  // man's location plus a spherical offset (yaw around, pitch up, cameraDistance
  // out); it always looks at the man (remember -Y is up)
  float cameraYaw = glm::pi<float>();  // start behind the man (his +Z is forward)
  float cameraPitch = 0.45f;           // elevation above the horizon, radians
  float cameraDistance = 4.5f;
  float mouseSensitivity = 0.005f;

  // Drag-to-orbit bookkeeping: cursor pos at the previous frame (for deltas) and
  // whether the left mouse button was held last frame
  glm::vec2 lastCursor{0.f};
  bool dragging = false;





  // The character is a GameObject carrying a TransformComponent (placement),
  // a SkinnedModelComponent (skinned model + clip playback), and
  // a KeyboardMovementComponent (WASD walk)

  // The component pointers are cached from addComponent so update() can drive them without a lookup
  GameObject man = GameObject::createGameObject();
  SkinnedModelComponent* manSkin = nullptr;
  KeyboardMovementComponent* manMover = nullptr;
  PlayerAbilityComponent* manAbility = nullptr;  // press F to launch a sphere.obj fireball
  RigidbodyComponent* manBody = nullptr;         // gravity + jumping; X/Z frozen for the mover
  ColliderComponent* manCollider = nullptr;      // stops him walking through the props

  // Space jumps, but only with his feet on something. Held here rather than in
  // the rigidbody because input is the scene's business, the same way the
  // fireball's aim is
  float jumpSpeed = 4.5f;  // upward, so it becomes -Y at the call site
  bool jumpPrevDown = false;

  // Half width of the man's collision box, in model units (the mesh's own bounds
  // give the height)

  // The static mesh pose gives the height, so the
  // footprint is pinned to manBodyRadius and outstretched arms don't widen it
  float manBodyRadius = 0.35f;






  std::unique_ptr<lve::LveModel> groundModel;  // models/quad.obj (flat XZ plane)
  std::unique_ptr<lve::LveModel> cubeModel;    // models/colored_cube.obj
  std::unique_ptr<lve::LveModel> blockModel;   // models/cube.obj (white)

  struct StaticProp {
    lve::LveModel* model = nullptr;
    glm::vec3 translation{0.f};
    glm::vec3 scale{1.f};
    glm::vec3 rotation{0.f};  // euler radians (matches the editor's TRS order)

    // Box collision sized from the model's mesh bounds
    // The scene refreshes the collision (fitPropColliders / refreshPropColliders)
    ColliderComponent collider{};
  };
  std::vector<StaticProp> props;

  ColliderComponent groundCollider{};
  float groundThickness = 1.f;

  // How far past a body's underside to look for a surface holding it up. Small
  // enough not to catch a floor it is genuinely falling toward, big enough to
  // survive a body settling a hair off the ground
  float groundProbeDepth = 0.03f;





  struct FallingBox {
    GameObject object = GameObject::createGameObject();
    RigidbodyComponent* body = nullptr;
    ColliderComponent* collider = nullptr;
    glm::vec3 spawn{0.f};  // where R puts it back
  };
  std::vector<FallingBox> boxes;

  void spawnFallingBoxes();
  void resetFallingBoxes();  // bound to R





  // --- Fireball impacts ------------------------------------------------------

  // A fireball that hits something hands its point light over to whatever it hit
  // Rather than caching a position, the light holds the collider it landed on and 
  // reads that box back every frame
  struct AttachedLight {
    const ColliderComponent* target = nullptr;  // null: a fixed spot (a ground hit)
    glm::vec3 fixedPosition{0.f};
    glm::vec3 color{1.f};
    float intensity = 0.f;
  };
  std::vector<AttachedLight> attachedLights;

  // Returns true when the fireball hits something solid
  // Hitting something makes the fireball disappear and leaves a light behind
  bool onFireballImpact(const PlayerAbilityComponent::Projectile& shot);

  // Attach a light to a target, or place it at a fixed spot if target is null
  void attachLight(const ColliderComponent* target, const glm::vec3& fixedPosition);

  // Finds where the light should sit: above the top of the target’s box
  glm::vec3 attachedLightPosition(const AttachedLight& light) const;

  float lightHoverHeight = 0.4f; // How high the light floats above the target

  glm::vec3 fireballLightColor{1.0f, 0.5f, 0.1f};
  float attachedLightIntensity = 5.f;

  // Max number of attached lights allowed.
  // If we go over, we drop the oldest one.
  std::size_t maxAttachedLights = 6;





  // Prop placement, shared by the draw and the collider so the box can never
  // drift from the mesh it is standing in for
  static glm::mat4 propMatrix(const StaticProp& prop);

  // Size each prop's box to its mesh, then project it into world space. Props
  // hold still, so both run once after loading; call refreshPropColliders again
  // if one is ever moved at runtime
  void fitPropColliders();
  void refreshPropColliders();

  // Push one body back out of everything solid it has moved into 
  // and tell its rigidbody about each shove so the velocity that 
  // drove it there gets cancelled or bounced
  void settleBody(GameObject& obj, ColliderComponent* collider, RigidbodyComponent* body);



  

  // Reads scene_layout.txt and reproduces here, resolving each line's preset name
  // to a model (the name is the model's file basename, e.g. "grass" -> models/grass.obj) 
  //
  // Models are owned by layoutModels, keyed by preset name so a mesh referenced 
  // by several objects is loaded only once
  void loadSceneLayout(const std::string& path);
  lve::LveModel* modelForPreset(const std::string& name);
  std::unordered_map<std::string, std::unique_ptr<lve::LveModel>> layoutModels;

  // Man's feet sit at world Y ~= manTranslation.y - manScale * yMin_model,
  // with yMin_model ~= -0.43 for man.glb. Tune if you swap the mesh
  float groundY = 0.5f;
  float groundHalfExtent = 8.f;  // quad spans [-1,1]; this scales it to +/-8
};
