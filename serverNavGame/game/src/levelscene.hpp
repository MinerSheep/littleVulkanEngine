
#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "keyboard_movement_controller.hpp"

class LevelScene : public lve::LveScene
{
public: 
    LevelScene() {}
    void update(float dt) override;
    void cleanup() override;

    void loadModels() override;
    void setupLights() override;

private:
 lve::LveCamera camera{};
 GameObject viewerObject;
 KeyboardMovementController cameraController{};

 GameObject::Map gameObjects;
};