
#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "keyboard_movement_controller.hpp"

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
};