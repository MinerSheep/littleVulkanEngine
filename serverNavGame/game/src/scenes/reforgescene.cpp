
#include <lve_engine.hpp>
#include <scenes/reforgescene.hpp>
#include <lve_frame_info.hpp>

#include <lve_model.hpp>
#include <iostream>
#include <random>



void ReforgeScene::update(float dt) 
{
    // Camera logic
    cameraController.moveInPlaneXZ(lve::LveEngine::instance().getGLFWWindow(), dt, *viewerObject);
    // lve::LveEngine::instance().setCamera(lve::LveEngine::Camera{ translation = viewerObject.transform.translation, rotation = viewerObject->transform.rotation});
    camera.setViewYXZ(viewerObject->getComponent<TransformComponent>()->translation, viewerObject->getComponent<TransformComponent>()->rotation);

    // This is responsible for maintaining object projection size
    // across different window aspect ratios
    float aspect = lve::LveEngine::instance().getAspectRatio();
    // camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

    static float cooldown = 0.0f;

    if (cooldown > 0.0f)
      cooldown -= dt;
    else if (glfwGetKey(lve::LveEngine::instance().getGLFWWindow(), GLFW_KEY_R) == GLFW_PRESS)
    {
      static std::mt19937 rng{std::random_device{}()};
      std::uniform_real_distribution<float> pickChannel(0.f, 1.f);
      
      int i = 0;
      for (auto& id : reforgeParts)
      {
        if (!reforgeModels[i].empty())
        {
          std::uniform_int_distribution<size_t> pickModel(0, reforgeModels[i % kReforgePieces].size() - 1);
  
          auto it = gameObjects.find(id);
          if (it != gameObjects.end()) {
            GameObject& obj = it->second;
  
            // randomize the model (shared_ptr, so parts can safely share one)
            obj.model = reforgeModels[i % kReforgePieces][pickModel(rng)];
  
            // randomize the color too
            obj.color = {pickChannel(rng), pickChannel(rng), pickChannel(rng)};
          }
        }

        i++;
      }

      cooldown = 1.0f;
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
        assert(transform);
        UIrenderItems.push_back({transform->mat2(), transform->translation, obj.color, obj.uiAlpha, obj.model.get()});
      }
      else
      {
        TransformComponent* transform = obj.getComponent<TransformComponent>();
        assert(transform);
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

void ReforgeScene::loadModels() 
{
    // used to store the camera's state
    static GameObject cameraObject = GameObject::createGameObject();
    viewerObject = &cameraObject;
    viewerObject->addComponent<TransformComponent>()->translation.z = -2.5f;

    // Preload the pool of models a reforge can randomly pick from.
    int i = 0;
    for (const char* path : {
             "models/hilt1.obj",
             "models/guard1.obj",
             "models/blade1.obj",
         }) {
      reforgeModels[i % kReforgePieces].push_back(lve::LveModel::createModelFromFile(path));
      i++;
    }

    std::shared_ptr<lve::LveModel> lveModel = lve::LveModel::createModelFromFile("models/hilt1.obj");
    {
      auto gameObj = GameObject::createGameObject();
      gameObj.model = lveModel;
      TransformComponent* transform = gameObj.addComponent<TransformComponent>();
      transform->translation = {-.5f, 1.5f, 0.f};
      transform->scale = glm::vec3(3.f);
      GameObject::id_t id = gameObj.getId();
      gameObjects.emplace(id, std::move(gameObj));

      reforgeParts.push_back(id);
    }

    lveModel = lve::LveModel::createModelFromFile("models/guard1.obj");
    {
      auto gameObj = GameObject::createGameObject();
      gameObj.model = lveModel;
      TransformComponent* transform = gameObj.addComponent<TransformComponent>();
      transform->translation = {-.5f, 1.5f, 0.f};
      transform->scale = glm::vec3(3.f);
      GameObject::id_t id = gameObj.getId();
      gameObjects.emplace(id, std::move(gameObj));

      reforgeParts.push_back(id);
    }

    lveModel = lve::LveModel::createModelFromFile("models/blade1.obj");
    {
      auto gameObj = GameObject::createGameObject();
      gameObj.model = lveModel;
      TransformComponent* transform = gameObj.addComponent<TransformComponent>();
      transform->translation = {-.5f, 1.5f, 0.f};
      transform->scale = glm::vec3(3.f);
      GameObject::id_t id = gameObj.getId();
      gameObjects.emplace(id, std::move(gameObj));

      reforgeParts.push_back(id);
    }

    {
      lveModel = lve::LveModel::createModelFromFile("models/quad.obj");
      auto floor = GameObject::createGameObject();
      floor.model = lveModel;
      TransformComponent* transform = floor.addComponent<TransformComponent>();
      transform->translation = {.0f, .5f, 0.f};
      transform->scale = glm::vec3{3.f,1.f,3.f};
      gameObjects.emplace(floor.getId(), std::move(floor));
    }
}

void ReforgeScene::setupLights() 
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
    auto pointLight = GameObject::makePointLight(0.2f);
    pointLight.color = lightColors[i];
    auto rotateLight = glm::rotate(
        glm::mat4(1.f),
        (i * glm::two_pi<float>()) / lightColors.size(),  // 360 degrees divided by 6
        {0.f, -1.f, 0.f});
    pointLight.getComponent<TransformComponent>()->translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
    gameObjects.emplace(pointLight.getId(), std::move(pointLight));
  }
}

void ReforgeScene::cleanup() 
{
  
}
