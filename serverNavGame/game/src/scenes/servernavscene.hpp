
#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "lve_text.hpp"
#include "keyboard_movement_controller.hpp"

#include "servernav_sim.hpp"

#include <memory>

class ServerNavScene : public lve::LveScene
{
public: 
    ServerNavScene() {}
    void update(float dt) override;
    void cleanup() override;

    void loadModels() override;
    void setupLights() override;

private:
 lve::LveCamera camera{};
 GameObject* viewerObject = nullptr;
 KeyboardMovementController cameraController{};

 std::unordered_map<id_t, GameObject> gameObjects;
 std::unordered_map<Vessel::id_t, GameObject::id_t> vesselMap;

 // dt is 2 mins a sec
 ServerNav nav = ServerNav::makeRandomScenario(8, 1, 120.0f);

 // Draws on-screen HUD text by appending solid dot-quads to UIrenderItems.
 // Created in loadModels() (after the device exists).
 std::unique_ptr<lve::LveTextRenderer> textRenderer;

 // Wall-clock accumulator that throttles the console navigation readout so
 // it doesn't flood the terminal every frame.
 float hudTimer = 0.f;
};