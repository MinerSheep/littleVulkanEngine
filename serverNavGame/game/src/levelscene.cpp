
#include <levelscene.hpp>
#include <lve_engine.hpp>
#include <lve_frame_info.hpp>

#include "game_object.hpp"

#include <lve_model.hpp>
#include <iostream>

void LevelScene::update(float dt) 
{
    // Camera logic
    auto it = gameObjects.find(camId);
    if (it != gameObjects.end()) {
        GameObject& viewerObject = it->second;
        cameraController.moveInPlaneXZ(lve::LveEngine::instance().getGLFWWindow(), dt, viewerObject);
        // lve::LveEngine::instance().setCamera(lve::LveEngine::Camera{ translation = viewerObject.transform.translation, rotation = viewerObject.transform.rotation});
        TransformComponent* camTransform = viewerObject.getComponent<TransformComponent>();
        camera.setViewYXZ(camTransform->translation, camTransform->rotation);
    }

    // This is responsible for maintaining object projection size
    // across different window aspect ratios
    float aspect = lve::LveEngine::instance().getAspectRatio();
    // camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

    // Set up render items before frameInfo
    renderItems.clear();
    for (auto& [id, obj] : gameObjects) {
      if (!obj.model) continue;

      obj.updateComponents(dt);

      TransformComponent* transform = obj.getComponent<TransformComponent>();

      renderItems.push_back({transform->mat4(), transform->normalMatrix(), obj.model.get()});
    }

    auto rotateLight = glm::rotate(glm::mat4(1.f), 0.5f * dt, {0.f, -1.f, 0.f});
    lightItems.clear();
    for (auto& [id, obj] : gameObjects) {
      if (PointLightComponent* pointLight = obj.getComponent<PointLightComponent>())
      {
        TransformComponent* transform = obj.getComponent<TransformComponent>();

        lve::LightRenderItem item;
        item.position = glm::vec3(rotateLight * glm::vec4(transform->translation, 1.f));
        item.color = obj.color;
        item.intensity = pointLight->lightIntensity;
        item.radius = transform->scale.x;
  
        auto offset = camera.getPosition() - item.position;
        item.distanceToCamera = glm::dot(offset, offset);
  
        lightItems.push_back(item);
      }

    }

    // update camera on the UBO
    ubo.projection = camera.getProjection();
    ubo.view = camera.getView();
    ubo.inverseView = camera.getInverseView();
}

void LevelScene::loadModels() 
{
    // used to store the camera's state
    {
      auto viewerObject = GameObject::createGameObject();
      viewerObject.getComponent<TransformComponent>()->translation = glm::vec3{0.f, 0.f, -2.5f};
      gameObjects.emplace(camId = viewerObject.getId(), std::move(viewerObject));
    }

    std::shared_ptr<lve::LveModel> lveModel = lve::LveModel::createModelFromFile("models/flat_vase.obj");

    {
      auto gameObj = GameObject::createGameObject();
      gameObj.model = lveModel;
      gameObj.getComponent<TransformComponent>()->translation = glm::vec3{-.5f, .5f, 0.f};
      gameObj.getComponent<TransformComponent>()->scale = glm::vec3(3.f);
      gameObjects.emplace(gameObj.getId(), std::move(gameObj));
    }

    lveModel = lve::LveModel::createModelFromFile("models/quad.obj");

    // floor
    {
      auto gameObj = GameObject::createGameObject();
      gameObj.model = lveModel;
      gameObj.getComponent<TransformComponent>()->translation = glm::vec3{.0f, .5f, 0.f};
      gameObj.getComponent<TransformComponent>()->scale = glm::vec3{3.f,1.f,3.f};
      gameObjects.emplace(gameObj.getId(), std::move(gameObj));
    }

}

void LevelScene::setupLights() 
{
  std::vector<glm::vec3> lightColors{
    {1.f, .1f, .1f},
    {.1f, .1f, 1.f},
    {.1f, 1.f, .1f},
    {1.f, 1.f, .1f},
    {.1f, 1.f, 1.f},
    {1.f, 1.f, 1.f}
  };
  
  // moving the variable with std::move means it becomes INACCESSIBLE, do not forget
  for (int i = 0; i < lightColors.size(); i++) {
    auto rotateLight = glm::rotate(
        glm::mat4(1.f),
        (i * glm::two_pi<float>()) / lightColors.size(),  // 360 degrees divided by 6
        {0.f, -1.f, 0.f});
    auto pointLight = GameObject::makePointLight(0.2f, glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f)));
    pointLight.color = lightColors[i];

    gameObjects.emplace(pointLight.getId(), std::move(pointLight));
  }
}

void LevelScene::cleanup() 
{

}
