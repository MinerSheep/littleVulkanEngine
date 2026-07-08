
#include <lve_engine.hpp>
#include <scenes/servernavscene.hpp>
#include <lve_frame_info.hpp>

#include <lve_model.hpp>
#include <iostream>



void ServerNavScene::update(float dt) 
{
    // Camera logic
    cameraController.moveInPlaneXZ(lve::LveEngine::instance().getGLFWWindow(), dt, *viewerObject);
    camera.setViewYXZ(viewerObject->getComponent<TransformComponent>()->translation, viewerObject->getComponent<TransformComponent>()->rotation);

    float aspect = lve::LveEngine::instance().getAspectRatio();
    // camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

    // Set up render items before frameInfo
    renderItems.clear();
    for (auto& [id, obj] : gameObjects) {
      if (!obj.model)
        continue;

      TransformComponent* transform = obj.getComponent<TransformComponent>();

      if (obj.UI)
        UIrenderItems.push_back({transform->mat2(), transform->translation, obj.color, obj.model.get()});
      else
        renderItems.push_back({transform->mat4(), transform->normalMatrix(), obj.model.get()});
    }
    
    lightItems.clear();
    for (auto& [id, obj] : gameObjects) {
      if (!obj.getComponent<PointLightComponent>()) continue;

      TransformComponent* transform = obj.getComponent<TransformComponent>();
      PointLightComponent* pointLight = obj.getComponent<PointLightComponent>();

      lve::LightRenderItem item;
      item.position = transform->translation;
      item.color = obj.color;
      item.intensity = pointLight->lightIntensity;
      item.radius = transform->scale.x;

      auto offset = camera.getPosition() - item.position;
      item.distanceToCamera = glm::dot(offset, offset);

      lightItems.push_back(item);
    }

    // update camera on the UBO
    ubo.projection = camera.getProjection();
    ubo.view = camera.getView();
    ubo.inverseView = camera.getInverseView();
}

void ServerNavScene::loadModels() 
{
    // used to store the camera's state
    static GameObject cameraObject = GameObject::createGameObject();
    viewerObject = &cameraObject;
    viewerObject->addComponent<TransformComponent>()->translation.z = -2.5f;

    // std::shared_ptr<lve::LveModel> lveModel = lve::LveModel::createModelFromFile("models/flat_vase.obj");
    // {
    //   auto gameObj = GameObject::createGameObject();
    //   gameObj.model = lveModel;
    //   TransformComponent* transform = gameObj.addComponent<TransformComponent>();
    //   transform->translation = {-.5f, .5f, 0.f};
    //   transform->scale = glm::vec3(3.f);
    //   gameObjects.emplace(gameObj.getId(), std::move(gameObj));
    // }
    std::vector<lve::LveModel::Vertex> vertices{
      {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}};

    for (auto& station : nav.stations)
    {
      auto ui = GameObject::createGameObject();
      ui.color = {1.0f, 0.0f, 0.0f};
      ui.model = std::make_unique<lve::LveModel>(lve::LveEngine::instance().getDevice(), lve::LveModel::Builder{vertices, {0,1,2}});
      
      TransformComponent* transform = ui.addComponent<TransformComponent>();
      transform->translation = {station.pos.x, station.pos.y, 0.0f};
      transform->scale = glm::vec3(1.f);
      ui.UI = true;
      gameObjects.emplace(ui.getId(), std::move(ui));

    }
}

void ServerNavScene::setupLights() 
{
}

void ServerNavScene::cleanup() 
{ 
}
