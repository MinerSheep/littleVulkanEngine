
#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "keyboard_movement_controller.hpp"

#include "game_object.hpp"

class LevelScene : public lve::LveScene
{
public: 
    LevelScene() {}
    void update(float dt) override;
    void cleanup() override;

    void loadModels() override;
    void setupLights() override;

private:
    GameObject::id_t camId;
    lve::LveCamera camera{};
    KeyboardMovementController cameraController{};

    GameObject::Map gameObjects;
};