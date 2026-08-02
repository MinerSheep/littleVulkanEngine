#include "petscop/room_scene.hpp"

#include <lve_engine.hpp>
#include <lve_skinned_model.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

void RoomScene::loadModels() {
  std::string error;
  if (!petscop::loadMap(mapPath, map, error)) throw std::runtime_error("[petscop] " + error);

  // Every mesh the map mentions, loaded once and kept for good
  // After this a door costs no disk reads and no uploads, just a swap of vectors
  models.preload(map.presets);

  textRenderer = std::make_unique<lve::LveTextRenderer>(lve::LveEngine::instance().getDevice());

  // --- the character ---------------------------------------------------------
  TransformComponent* xform = player.addComponent<TransformComponent>();
  xform->translation = {0.f, groundY, 0.f};
  xform->scale = glm::vec3(0.9f);
  xform->rotation = {0.f, 0.f, glm::pi<float>()};  // glTF Y-up -> engine Y-down

  playerSkin = player.addComponent<SkinnedModelComponent>();
  playerSkin->setModel(lve::LveSkinnedModel::createModelFromFile("models/statue.glb"));

  // Forward is set once per room from where the camera sits, so WASD stays
  // camera relative even though the camera never moves
  playerMover = player.addComponent<KeyboardMovementComponent>();
  playerMover->setAnimations(playerSkin, 3, 0);

  // Gravity, added after movement so the two never argue over a frame
  playerBody = player.addComponent<RigidbodyComponent>();
  playerBody->freezePositionX = true;  // the mover writes X and Z itself
  playerBody->freezePositionZ = true;

  // Box last, so it ticks after the move and ends the frame where he really is
  playerCollider = player.addComponent<ColliderComponent>();
  playerCollider->isStatic = false;
  if (playerSkin->model) {
    playerCollider->fitToModel(*playerSkin->model);
    playerCollider->localHalfExtent.x = glm::min(playerCollider->localHalfExtent.x, playerRadius);
    playerCollider->localHalfExtent.z = glm::min(playerCollider->localHalfExtent.z, playerRadius);
  }

  std::cout << "[petscop] loaded map '" << map.name << "': " << map.rooms.size() << " room(s), "
            << map.presets.size() << " mesh(es)" << std::endl;

  enterRoom(map.startRoom, -1);
}

glm::mat4 RoomScene::placement(const glm::vec3& t, const glm::vec3& r, const glm::vec3& s) {
  glm::mat4 mat = glm::translate(glm::mat4(1.f), t);
  mat = glm::rotate(mat, r.y, glm::vec3(0.f, 1.f, 0.f));
  mat = glm::rotate(mat, r.x, glm::vec3(1.f, 0.f, 0.f));
  mat = glm::rotate(mat, r.z, glm::vec3(0.f, 0.f, 1.f));
  return glm::scale(mat, s);
}

void RoomScene::enterRoom(int roomIndex, int arriveDoor) {
  const petscop::MapRoom& room = map.rooms[roomIndex];

  // Let go of every box FIRST
  // The collision system keeps raw pointers into props, so it has to be emptied
  // before anything in props is destroyed
  collisions.clear();
  props.clear();
  doors.clear();

  // Sized up front, so filling them never moves what is already in
  props.reserve(room.objects.size());
  doors.reserve(room.doors.size());

  for (const petscop::MapObject& object : room.objects) {
    Prop prop;
    prop.model = models.get(map.presets[object.preset]);
    prop.translation = object.translation;
    prop.rotation = object.rotation;
    prop.scale = object.scale;
    prop.face = object.face;
    props.push_back(prop);
  }

  for (const petscop::MapDoor& source : room.doors) {
    Door door;
    door.translation = source.translation;
    door.rotation = source.rotation;
    door.scale = source.scale;
    door.toRoom = source.toRoom;
    door.toDoor = source.toDoor;
    door.spawn = source.spawn;
    door.spawnYaw = source.spawnYaw;
    doors.push_back(door);
  }

  // Boxes, now that nothing more is going to be added
  for (Prop& prop : props) {
    if (prop.model) prop.collider.fitToModel(*prop.model);
    prop.collider.refresh(placement(prop.translation, prop.rotation, prop.scale));
  }
  for (Door& door : doors) {
    // A doorway has no mesh, so its box is just the opening. The map stores the
    // opening as a half size, the same way a cube's scale is its half size
    door.trigger.setLocalBox(glm::vec3(0.f), glm::vec3(1.f));
    door.trigger.refresh(placement(door.translation, door.rotation, door.scale));
  }

  // Stand him at the door he came through, or in the middle for the first room
  TransformComponent* xform = player.getComponent<TransformComponent>();
  const bool arrived = arriveDoor >= 0 && arriveDoor < static_cast<int>(doors.size());
  if (arrived) {
    xform->translation = doors[arriveDoor].spawn;
    xform->rotation.y = doors[arriveDoor].spawnYaw;
  } else {
    xform->translation = glm::vec3(0.f, groundY, 0.f);
    xform->rotation.y = 0.f;
  }
  if (playerBody) {
    playerBody->velocity = glm::vec3(0.f);
    playerBody->grounded = false;
  }
  if (playerCollider) playerCollider->refresh(xform->mat4());

  // The camera holds still in a room, so forward is worked out once here rather
  // than every frame. W walks away from the camera
  cameraEye = room.cameraEye;
  cameraLook = room.cameraLook;
  const glm::vec3 look = cameraLook - cameraEye;
  if (playerMover) playerMover->forwardYaw = std::atan2(look.x, look.z);

  // Register LAST, now that props and doors have stopped growing
  // Doorways are left out on purpose, they are meant to be walked through
  for (const Prop& prop : props) collisions.addStatic(&prop.collider);
  collisions.addDynamic(player, playerCollider, playerBody);

  // He comes out standing in a doorway, so that one waits until he steps off it
  // Without this he would be sent straight back where he came from
  for (Door& door : doors) door.armed = true;
  if (arrived) doors[arriveDoor].armed = false;

  currentRoom = roomIndex;
  std::cout << "[petscop] entered " << room.name << ": " << props.size() << " prop(s), "
            << doors.size() << " door(s)" << std::endl;
}

void RoomScene::updateWallVisibility() {
  const glm::vec3 look = cameraLook - cameraEye;
  if (glm::length(look) < 1e-4f) return;

  // The question is really which side of the room the camera is standing on, so
  // ask it on the ground plane. The camera tilts down to see the floor, and that
  // tilt otherwise eats into every wall's score until a steep enough camera
  // starts dropping the far wall as well
  glm::vec2 forwardFlat(look.x, look.z);
  const bool flatUsable = glm::length(forwardFlat) > 1e-4f;
  if (flatUsable) forwardFlat = glm::normalize(forwardFlat);
  const glm::vec3 forward = glm::normalize(look);

  for (Prop& prop : props) {
    // Only walls say which way they face, so everything else always draws
    if (prop.face == glm::vec3(0.f)) {
      prop.visibility = 1.f;
      continue;
    }

    glm::vec2 outwardFlat(prop.face.x, prop.face.z);
    float facing;
    if (flatUsable && glm::length(outwardFlat) > 1e-4f) {
      facing = glm::dot(glm::normalize(outwardFlat), forwardFlat);
    } else {
      // A floor or a ceiling has no sideways direction, so ask in full 3D
      facing = glm::dot(glm::normalize(prop.face), forward);
    }

    float ghostAlpha = 0.25f;
    prop.visibility = facing < hideThreshold ? ghostAlpha : 1.f;
  }
}

void RoomScene::update(float dt) {
  if (phase == Phase::Playing) {
    GLFWwindow* window = lve::LveEngine::instance().getGLFWWindow();

    // Space jumps, one per press and only with his feet on something. It goes in
    // as a velocity change so height does not depend on his mass, and it is
    // negative because -Y is up. Must land before the components tick
    const bool jumpDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (jumpDown && !jumpPrevDown && playerBody && playerBody->grounded) {
      playerBody->addForce(glm::vec3(0.f, -jumpSpeed, 0.f),
                           RigidbodyComponent::ForceMode::VelocityChange);
      playerBody->grounded = false;
    }
    jumpPrevDown = jumpDown;

    player.updateComponents(dt);

    // He has moved and his box has followed, so shove him back out of the walls
    collisions.settleAll();

    if (playerCollider) {
      const ColliderComponent::Aabb& box = playerCollider->worldBox();

      // The door he walked out of goes live again once he is clear of it
      for (Door& door : doors) {
        if (!door.armed && !box.overlaps(door.trigger.worldBox())) door.armed = true;
      }

      // Standing in a doorway starts the fade. The room is NOT swapped here:
      // props and doors are still being read further down this frame
      for (std::size_t i = 0; i < doors.size(); i++) {
        const Door& door = doors[i];
        if (!door.armed || door.toRoom < 0) continue;
        if (!box.overlaps(door.trigger.worldBox())) continue;

        pendingRoom = door.toRoom;
        pendingDoor = door.toDoor;
        phase = Phase::FadingOut;
        break;
      }
    }

  } else if (phase == Phase::FadingOut) {
    // He is frozen here on purpose, otherwise he keeps walking out through the
    // doorway while the screen is black
    fade += dt / fadeSeconds;
    if (fade >= 1.f) {
      fade = 1.f;

      // The one place the room is ever rebuilt. Everything that reads props and
      // doors has finished for the frame, so their boxes are safe to throw away
      enterRoom(pendingRoom, pendingDoor);
      pendingRoom = -1;
      pendingDoor = -1;
      phase = Phase::FadingIn;
    }

  } else {
    fade -= dt / fadeSeconds;
    if (fade <= 0.f) {
      fade = 0.f;
      phase = Phase::Playing;
    }
  }

  // --- camera: one pose for the whole room ------------------------------------
  camera.setViewTarget(cameraEye, cameraLook);
  camera.setPerspectiveProjection(glm::radians(50.f), lve::LveEngine::instance().getAspectRatio(),
                                  0.1f, 100.f);

  // --- draw lists, rebuilt each frame for the engine to pack into a FrameInfo --
  skinnedRenderItems.clear();
  collectSkinned(player, skinnedRenderItems);

  // Done every frame rather than on the way into the room, so that a camera that
  // moves one day needs nothing changed here
  updateWallVisibility();

  renderItems.clear();
  for (const Prop& prop : props) {
    if (!prop.model) continue;

    // A wall standing between the camera and the room comes back ghosted rather
    // than solid, see updateWallVisibility. Either way it still collides, its box
    // was registered on the way in
    // Set a wall's visibility to 0 and it drops out of the draw entirely
    if (prop.visibility <= 0.f) continue;

    const glm::mat4 mat = placement(prop.translation, prop.rotation, prop.scale);
    const glm::mat4 normal = glm::mat4(glm::transpose(glm::inverse(glm::mat3(mat))));
    renderItems.push_back({mat, normal, prop.model, prop.visibility});
  }

  // Only what this room asked for. Anything past the budget is dropped rather
  // than fighting the ones already in
  lightItems.clear();
  if (currentRoom >= 0) {
    for (const petscop::MapLight& light : map.rooms[currentRoom].lights) {
      if (lightItems.size() >= MAX_LIGHTS) break;

      lve::LightRenderItem item;
      item.position = light.position;
      item.color = light.color;
      item.intensity = light.intensity;
      item.radius = 0.1f;
      const glm::vec3 offset = camera.getPosition() - light.position;
      item.distanceToCamera = glm::dot(offset, offset);
      lightItems.push_back(item);
    }
  }

  // --- overlay: the room's name, then the fade over the top of it -------------
  UIrenderItems.clear();
  if (textRenderer) {
    if (currentRoom >= 0) {
      textRenderer->emit(UIrenderItems, map.rooms[currentRoom].name, glm::vec2(-0.95f, -0.93f),
                         0.035f, glm::vec3(0.85f));
    }
    if (fade > 0.f) {
      // One quad over the whole screen. The unit quad runs 0 to 1, so doubling it
      // and shifting back by one covers -1 to 1 both ways
      // Pushed last, because UI draws in the order it is handed over
      UIrenderItems.push_back({glm::mat2(2.f, 0.f, 0.f, 2.f), glm::vec2(-1.f), glm::vec3(0.f), fade,
                               textRenderer->quad()});
    }
  }

  ubo.ambientLightColor = {1.f, 1.f, 1.f, 0.15f};
  ubo.projection = camera.getProjection();
  ubo.view = camera.getView();
  ubo.inverseView = camera.getInverseView();
}

void RoomScene::cleanup() {
  // Drop the boxes before the vectors holding them go
  collisions.clear();
  props.clear();
  doors.clear();
}
