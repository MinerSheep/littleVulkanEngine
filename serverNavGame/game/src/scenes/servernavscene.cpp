
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

    // Update logic
    nav.update(dt);
    for (auto& vessel : nav.vessels)
    {
      GameObject::id_t id = vesselMap[vessel.id];

      auto it = gameObjects.find(id);
      if (it != gameObjects.end()) {
          GameObject& obj = it->second;

          glm::vec2 position = vessel.pos / static_cast<float>(kGridSize - 1);
          obj.getComponent<RectTransformComponent>()->translation = position * 2.0f; 
      }
    }
    
    // Set up render items before frameInfo
    renderItems.clear();
    UIrenderItems.clear();
    for (auto& [id, obj] : gameObjects) {
      if (!obj.model)
        continue;

      if (obj.UI)
      {
        RectTransformComponent* transform = obj.getComponent<RectTransformComponent>();
        UIrenderItems.push_back({transform->mat2(), transform->anchorNdc(transform->anchor) + transform->translation, obj.color, 0.5f, obj.model.get()});
      }
      else
      {
        TransformComponent* transform = obj.getComponent<TransformComponent>();
        renderItems.push_back({transform->mat4(), transform->normalMatrix(), obj.model.get()});
      }
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
    
    // Weather arrows
    {
      std::vector<lve::LveModel::Vertex> vertices{
        // Arrow head (tip)
        {{0.0f,  0.3f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        // Left base of head
        {{-0.15f, 0.1f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        // Right base of head
        {{0.15f,  0.1f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        // Tail left
        {{-0.075f, 0.1f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        // Tail right
        {{0.075f, 0.1f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        // Tail bottom
        {{0.0f, -0.3f, 0.0f}, {0.0f, 0.0f, 1.0f}},
      };
      std::shared_ptr<lve::LveModel> lveModel = std::make_shared<lve::LveModel>(lve::LveEngine::instance().getDevice(), lve::LveModel::Builder{vertices, {0,1,2}});
      for (int i = 0; i < kGridSize; i++)
      {
        for (int j = 0; j < kGridSize; j++)
        {
          WeatherCell cell = nav.map[i][j];
          auto ui = GameObject::createGameObject();
          ui.model = lveModel;
          float strength = cell.weight / 2.0f;
          ui.color = {glm::mix(0.0f, 1.0f, strength), 0.0f, glm::mix(1.0f, 0.0f, strength)};
          ui.UI = true;
          
          // scaled from 0 to kGridSize which is 49.f
          glm::vec2 position = glm::vec2{i,j} / static_cast<float>(kGridSize - 1);
  
          RectTransformComponent* transform = ui.addComponent<RectTransformComponent>();
          transform->anchor = RectTransformComponent::UIAnchor::TopLeft;
          transform->translation = {position.x * 2.0f, position.y * 2.0f};
          transform->scale = glm::vec3(.05f);
          gameObjects.emplace(ui.getId(), std::move(ui));
        }
      }
    }

    std::vector<lve::LveModel::Vertex> vertices{
      {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    std::shared_ptr<lve::LveModel> lveModel = std::make_shared<lve::LveModel>(lve::LveEngine::instance().getDevice(), lve::LveModel::Builder{vertices, {0,1,2}});
    
    for (auto& station : nav.stations)
    {
      auto ui = GameObject::createGameObject();
      ui.model = lveModel;
      ui.color = {1.0f, 0.0f, 0.0f};
      ui.UI = true;
      
      // scaled from 0 to kGridSize which is 49.f
      glm::vec2 position = station.pos / static_cast<float>(kGridSize - 1);

      RectTransformComponent* transform = ui.addComponent<RectTransformComponent>();
      transform->anchor = RectTransformComponent::UIAnchor::TopLeft;
      transform->translation = {position.x * 2.0f, position.y * 2.0f};
      transform->scale = glm::vec3(.1f);
      gameObjects.emplace(ui.getId(), std::move(ui));
      
    }

    for (auto& vessel : nav.vessels)
    {
      auto ui = GameObject::createGameObject();
      ui.model = lveModel;
      ui.color = {0.0f, 1.0f, 0.0f};
      ui.UI = true;
      
      // scaled from 0 to kGridSize which is 49.f
      glm::vec2 position = vessel.pos / static_cast<float>(kGridSize - 1);

      RectTransformComponent* transform = ui.addComponent<RectTransformComponent>();
      transform->anchor = RectTransformComponent::UIAnchor::TopLeft;
      transform->translation = {position.x * 2.0f, position.y * 2.0f};
      transform->scale = glm::vec3(.1f);

      GameObject::id_t id = ui.getId();
      gameObjects.emplace(id, std::move(ui));
      vesselMap[vessel.id] = id;
    }
}

void ServerNavScene::setupLights() 
{
}

void ServerNavScene::cleanup() 
{ 
}
