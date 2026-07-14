
#pragma once

#include "lve_scene.hpp"
#include "lve_camera.hpp"
#include "keyboard_movement_controller.hpp"

class ReforgeScene : public lve::LveScene
{
public: 
    ReforgeScene() {}
    void update(float dt) override;
    void cleanup() override;

    void loadModels() override;
    void setupLights() override;

private:
 lve::LveCamera camera{};
 GameObject* viewerObject = nullptr;
 KeyboardMovementController cameraController{};

 std::unordered_map<id_t, GameObject> gameObjects;

 std::vector<GameObject::id_t> reforgeParts;

 // pool of models a reforge can pick from (loaded once in loadModels)
 std::vector<std::shared_ptr<lve::LveModel>> reforgeModels;
};