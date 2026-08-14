#include "petscop/room_scene.hpp"

#include <lve_engine.hpp>
#include <lve_skinned_model.hpp>

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

// The room's name the way it goes on screen
// A room ident cannot hold a space, so Hall_Main reads back as Hall Main
std::string displayName(const std::string& ident) {
  std::string out = ident;
  for (char& c : out) {
    if (c == '_') c = ' ';
  }
  return out;
}

}  // namespace

void RoomScene::loadModels() {
  std::string error;
  if (!petscop::loadMap(mapPath, map, error)) 
    throw std::runtime_error("[petscop] " + error);

  // Every mesh in the map is loaded and stored permanently
  // A door is preloaded, and swapped in place of every door
  models.preload(map.presets);

  // No file yet is a new game, not a fault
  if (petscop::readSave(savePath, state))
    std::cout << "[petscop] loaded save '" << savePath << "': " << state.items.size()
              << " item(s), " << state.flags.size() << " flag(s)" << std::endl;

  textRenderer = std::make_unique<lve::LveTextRenderer>(lve::LveEngine::instance().getDevice());

  // --- the character ---------------------------------------------------------
  TransformComponent* xform = player.addComponent<TransformComponent>();
  xform->translation = {0.f, groundY, 0.f};
  xform->scale = glm::vec3(0.9f);
  xform->rotation = {0.f, 0.f, glm::pi<float>()};  // glTF Y-up -> engine Y-down

  playerSkin = player.addComponent<SkinnedModelComponent>();
  playerSkin->setModel(lve::LveSkinnedModel::createModelFromFile("models/statue.glb"));

  // Forward is set once per room from where the camera sits
  playerMover = player.addComponent<KeyboardMovementComponent>();
  playerMover->setAnimations(playerSkin, 3, 0);

  // Gravity, add after movement
  playerBody = player.addComponent<RigidbodyComponent>();
  playerBody->freezePositionX = true;  // the mover writes X and Z itself
  playerBody->freezePositionZ = true;

  // Box last, it ticks after the move and ends the frame where the player really is
  playerCollider = player.addComponent<ColliderComponent>();
  playerCollider->isStatic = false;
  if (playerSkin->model) {
    playerCollider->fitToModel(*playerSkin->model);
    playerCollider->localHalfExtent.x = glm::min(playerCollider->localHalfExtent.x, playerRadius);
    playerCollider->localHalfExtent.z = glm::min(playerCollider->localHalfExtent.z, playerRadius);
  }

  // The props vector never moves, only what is inside it, so this is bound once
  interactions.bind(&props, &dialog, &state);

  // Same again for the spooky events, which read the map and the save as well
  petscop::Stage stage;
  stage.map = &map;
  stage.state = &state;
  stage.props = &props;
  stage.models = &models;
  stage.player = xform;
  stage.dialog = &dialog;
  events.bind(stage);

  std::cout << "[petscop] loaded map '" << map.name << "': " << map.rooms.size() << " room(s), "
            << map.presets.size() << " mesh(es)" << std::endl;

  enterRoom(startingRoom(), -1);
}

int RoomScene::startingRoom() {
  int found = map.startRoom;
  for (std::size_t i = 0; i < map.rooms.size(); i++) {
    if (map.rooms[i].name == state.room) found = static_cast<int>(i);
  }

  // An event gets to say he woke up somewhere else
  return events.wakeRoom(found);
}

void RoomScene::enterRoom(int roomIndex, int arriveDoor) {
  // An event gets first refusal on the room, and may hand back a changed copy
  const petscop::MapRoom& room = events.dress(map.rooms[roomIndex], roomIndex);
  liveRoom = &room;

  // Write the old room down before any of it is thrown away
  if (currentRoom >= 0) state.rememberRoom(map.rooms[currentRoom].name, props);

  // Clear collisions FIRST, they carry raw pointers
  collisions.clear();

  // then clear boxes
  props.clear();
  doors.clear();

  interactions.reset();
  if (playerMover) playerMover->enabled = true;

  props.reserve(room.objects.size());
  doors.reserve(room.doors.size());

  for (const petscop::MapObject& object : room.objects) {
    Prop prop;
    prop.model = models.get(map.presets[object.preset]);
    prop.translation = object.translation;
    prop.rotation = object.rotation;
    prop.scale = object.scale;
    prop.face = object.face;
    prop.solid = object.solid;

    // Objects expanded to have actions
    // Therefore we now hold default states
    prop.restTranslation = object.translation;
    prop.restRotation = object.rotation;
    prop.restScale = object.scale;
    prop.name = object.name;
    prop.actions = object.actions;
    prop.flipped.assign(object.actions.size(), 0);
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
    prop.refreshBox();
  }

  // The map only says where things started. Anything moved, opened or taken on an
  // earlier visit is put back the way it was left
  state.restoreRoom(room.name, props);

  for (Door& door : doors) {
    // A doorway has no mesh, just a box opening at half size
    door.trigger.setLocalBox(glm::vec3(0.f), glm::vec3(1.f));
    door.trigger.refresh(petscop::placement(door.translation, door.rotation, door.scale));

    // Same box again, off until something locks the doors
    door.blocker.setLocalBox(glm::vec3(0.f), glm::vec3(1.f));
    door.blocker.refresh(petscop::placement(door.translation, door.rotation, door.scale));
    door.blocker.enabled = false;
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

  cameraEye = room.cameraEye;
  cameraLook = room.cameraLook;
  const glm::vec3 look = cameraLook - cameraEye;
  if (playerMover) playerMover->forwardYaw = std::atan2(look.x, look.z);

  // Register LAST, now that props and doors have stopped growing
  // Doorways are left out on purpose, they are meant to be walked through
  for (const Prop& prop : props) {
    if (prop.solid) collisions.addStatic(&prop.collider);
  }

  // The plugs go in disabled, so a doorway is still a doorway until it is not
  for (const Door& door : doors) collisions.addStatic(&door.blocker);

  collisions.addDynamic(player, playerCollider, playerBody);

  // He comes out standing in a doorway, so that one waits until he steps off it
  // Without this he would be sent straight back where he came from
  for (Door& door : doors) door.armed = true;
  if (arrived) doors[arriveDoor].armed = false;

  currentRoom = roomIndex;

  // Last, so an event can put props right now that they are all standing
  events.onEnterRoom(roomIndex);

  // Walking through a door is the autosave
  state.room = room.name;
  petscop::writeSave(savePath, state);

  std::cout << "[petscop] entered " << room.name << ": " << props.size() << " prop(s), "
            << doors.size() << " door(s)" << std::endl;
}

void RoomScene::updateWallVisibility() {
  const glm::vec3 look = cameraLook - cameraEye;
  if (glm::length(look) < 1e-4f) return;

  // The camera tilts down to see the floor from the ground
  // Not really sure how this works
  glm::vec2 forwardFlat(look.x, look.z);

  const bool flatUsable = glm::length(forwardFlat) > 1e-4f;
  if (flatUsable) forwardFlat = glm::normalize(forwardFlat);

  const glm::vec3 forward = glm::normalize(look);

  for (Prop& prop : props) {
    // Walls have a face and will hit this if check
    if (prop.face == glm::vec3(0.f)) {
      prop.visibility = 1.f;
      continue;
    }

    glm::vec2 outwardFlat(prop.face.x, prop.face.z);
    float facing;
    if (flatUsable && glm::length(outwardFlat) > 1e-4f) {
      facing = glm::dot(glm::normalize(outwardFlat), forwardFlat);
    } else {
      // A floor or a ceiling has no sideways direction, we check in 3d
      facing = glm::dot(glm::normalize(prop.face), forward);
    }

    float ghostAlpha = 0.25f;
    prop.visibility = facing < hideThreshold ? ghostAlpha : 1.f;
  }
}

// Floating box to signify when you got close to an object you can interact with
void RoomScene::emitHoverBox() {
  if (!textRenderer) return;

  const int nearProp = interactions.nearProp();
  if (nearProp < 0 || nearProp >= static_cast<int>(props.size())) return;

  const ColliderComponent::Aabb& box = props[nearProp].collider.worldBox();
  
  const float topY = glm::max(box.min.y, groundY - hoverMaxHeight);
  const float bob = hoverBob * std::sin(clock * hoverBobSpeed);
  const glm::vec3 world{box.center().x, topY - hoverLift - bob, box.center().z};

  const glm::vec4 clip = camera.getProjection() * camera.getView() * glm::vec4(world, 1.f);
  if (clip.w <= 1e-4f) return;

  petscop::emitHoverPrompt(UIrenderItems, *textRenderer, glm::vec2(clip.x / clip.w, clip.y / clip.w),
                           "E", hoverDotHeight, 1.f);
}

// The room floats over a backdrop that never stops moving
void RoomScene::emitBackground() {
  backgroundItems.clear();
  if (!bgEnabled || !textRenderer) return;

  // The unit quad runs 0 to 1
  // doubling it and shifting it covers the whole screen, the same way the fade quad does
  backgroundItems.push_back(
      {glm::mat2(2.f, 0.f, 0.f, 2.f), glm::vec2(-1.f), bgWash, 1.f, textRenderer->quad()});

  // How much screen one bar owns, and how far along its slot it has slid
  // fmod wraps the slide inside a single slot, so when it snaps back to zero
  // bar i lands exactly where bar i-1 was and the march has no seam
  const float slot = 2.f / static_cast<float>(bgBars);
  const float slide = std::fmod(bgPhase, slot);

  // A mat2 is column major: the first pair is where the quad's x goes, the
  // second is where its y goes
  const glm::mat2 lean{slot * bgBarFill, 0.f, bgSlant, 2.f};

  // Bars have to start off screen or the leading corner shows bare wash
  const int lead = 1 + static_cast<int>(std::ceil(std::fabs(bgSlant) / slot));

  for (int i = -lead; i <= bgBars + 1; i++) {
    const float x = -1.f + slide + static_cast<float>(i) * slot;
    backgroundItems.push_back({lean, glm::vec2(x, -1.f), bgBar, 1.f, textRenderer->quad()});
  }
}

void RoomScene::update(float dt) {
  clock += dt;

  if (phase == Phase::Playing) {
    GLFWwindow* window = lve::LveEngine::instance().getGLFWWindow();

    const bool jumpDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (jumpDown && !jumpPrevDown && playerBody && playerBody->grounded) {

      playerBody->addForce(glm::vec3(0.f, -jumpSpeed, 0.f), RigidbodyComponent::ForceMode::VelocityChange);
      playerBody->grounded = false;
    }
    jumpPrevDown = jumpDown;

    petscop::tickMotions(props, dt);

    if (playerMover && events.takesControl()) playerMover->enabled = false;

    // A held door is a wall you cannot walk out through, not just a dead trigger
    // One door on its own can be plugged too, for a way on that is not open yet
    const bool sealed = events.locksDoors();
    for (std::size_t i = 0; i < doors.size(); i++)
      doors[i].blocker.enabled = sealed || static_cast<int>(i) == events.sealedDoor();

    player.updateComponents(dt);

    // He has moved and his box has followed, so shove him back out of the walls
    collisions.settleAll();

    interactions.update(playerCollider, playerMover);

    if (playerCollider && !dialog.isOpen()) {
      const ColliderComponent::Aabb& box = playerCollider->worldBox();

      // The door he walked out of goes live again once he is clear of it
      for (Door& door : doors) {
        if (!door.armed && !box.overlaps(door.trigger.worldBox())) door.armed = true;
      }

      // Standing in a doorway starts the fade. The room is NOT swapped here:
      // props and doors are still being read further down this frame
      // An event can hold every door shut, and can send one somewhere else
      for (std::size_t i = 0; i < doors.size() && !events.locksDoors(); i++) {
        const Door& door = doors[i];
        if (!door.armed || door.toRoom < 0) continue;
        if (!box.overlaps(door.trigger.worldBox())) continue;

        pendingRoom = door.toRoom;
        pendingDoor = door.toDoor;

        // A refused door has to be stepped off before it is tried again
        if (!events.reroute(pendingRoom, pendingDoor)) {
          doors[i].armed = false;
          pendingRoom = -1;
          pendingDoor = -1;
          break;
        }

        phase = Phase::FadingOut;
        break;
      }
    }

    // An event may move him with no door involved at all
    int warpRoom = -1;
    int warpDoor = -1;
    if (phase == Phase::Playing && events.takeWarp(warpRoom, warpDoor)) {
      pendingRoom = warpRoom;
      pendingDoor = warpDoor;
      phase = Phase::FadingOut;
    }

  } else if (phase == Phase::FadingOut) {
    // player is frozen here
    fade += dt / fadeSeconds;
    if (fade >= 1.f) {
      fade = 1.f;

      // The one place the room is ever rebuilt. Everything that reads props and
      // doors has finished for the frame
      enterRoom(pendingRoom, pendingDoor);
      pendingRoom = -1;
      pendingDoor = -1;
      phase = Phase::FadingIn;
    }

  } else if (!events.holdsBlack()) {
    // An event can keep the screen shut after the room has already been swapped
    fade -= dt / fadeSeconds;
    if (fade <= 0.f) {
      fade = 0.f;
      phase = Phase::Playing;
    }
  }

  // Runs whatever the phase, so a held fade and a room swap both still tick
  events.update(dt, phase == Phase::Playing,
                phase == Phase::Playing ? interactions.startedProp() : -1);

  // --- camera: one pose for the whole room, unless an event has taken it ------
  events.cameraOverride(cameraEye, cameraLook);
  camera.setViewTarget(cameraEye, cameraLook);
  camera.setPerspectiveProjection(glm::radians(50.f), lve::LveEngine::instance().getAspectRatio(),
                                  0.1f, 100.f);

  // --- draw lists, rebuilt each frame for the engine to pack into a FrameInfo --
  skinnedRenderItems.clear();
  collectSkinned(player, skinnedRenderItems);

  updateWallVisibility();

  renderItems.clear();
  for (const Prop& prop : props) {
    if (!prop.model) continue;
    if (prop.disappeared) continue;

    // A wall standing between the camera and the room comes back ghosted rather
    // than solid, but this is a double check
    if (prop.visibility <= 0.f) continue;

    const glm::mat4 mat = prop.matrix();
    const glm::mat4 normal = glm::mat4(glm::transpose(glm::inverse(glm::mat3(mat))));
    renderItems.push_back({mat, normal, prop.model, prop.visibility});
  }

  // Anything an event conjured. These have no box and nothing walks into them
  for (const Prop& extra : events.extras()) {
    if (!extra.model) continue;

    const glm::mat4 mat = extra.matrix();
    const glm::mat4 normal = glm::mat4(glm::transpose(glm::inverse(glm::mat3(mat))));
    renderItems.push_back({mat, normal, extra.model, 1.f});
  }

  // Only what this room asked for. Anything past the budget is dropped rather
  // than fighting the ones already in
  lightItems.clear();
  if (liveRoom) {
    for (std::size_t i = 0; i < liveRoom->lights.size(); i++) {
      if (lightItems.size() >= MAX_LIGHTS) break;

      // An event can pull a room's lights down, or put one out for good
      const float gain = events.lightGain(i);
      if (gain <= 0.001f) continue;

      const petscop::MapLight& light = liveRoom->lights[i];
      lve::LightRenderItem item;
      item.position = light.position;
      item.color = light.color;
      item.intensity = light.intensity * gain;
      item.radius = 0.1f;
      const glm::vec3 offset = camera.getPosition() - light.position;
      item.distanceToCamera = glm::dot(offset, offset);
      lightItems.push_back(item);
    }
  }

  // --- the backdrop, under everything the room draws --------------------------
  bgPhase += dt * events.backgroundSpeed(bgSpeed);
  emitBackground();

  // --- overlay: the room's name, then the fade over the top of it -------------
  UIrenderItems.clear();
  if (textRenderer) {
    if (currentRoom >= 0) {
      textRenderer->emit(UIrenderItems, displayName(map.rooms[currentRoom].name),
                         glm::vec2(-0.95f, -0.93f), 0.035f, glm::vec3(0.85f));
    }
    if (phase == Phase::Playing) {
      emitHoverBox();
      dialog.emit(UIrenderItems, *textRenderer);
    }
    if (fade > 0.f) {
      // One quad over the whole screen
      // Pushed last, because UI draws in the order it is handed over
      UIrenderItems.push_back({glm::mat2(2.f, 0.f, 0.f, 2.f), glm::vec2(-1.f), glm::vec3(0.f), fade,
                               textRenderer->quad()});
    }
  }

  ubo.ambientLightColor = events.ambient(glm::vec4(1.f, 1.f, 1.f, 0.15f));
  ubo.projection = camera.getProjection();
  ubo.view = camera.getView();
  ubo.inverseView = camera.getInverseView();
}

void RoomScene::cleanup() {
  // Last chance to write down the room he was standing in
  if (currentRoom >= 0) {
    state.rememberRoom(map.rooms[currentRoom].name, props);
    petscop::writeSave(savePath, state);
    currentRoom = -1;
  }

  // Drop the boxes before the vectors holding them go
  collisions.clear();
  props.clear();
  doors.clear();
  interactions.reset();
  events.reset();
  liveRoom = nullptr;
  backgroundItems.clear();
}
