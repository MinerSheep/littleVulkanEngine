
#include <lve_engine.hpp>
#include <levelscene.hpp>
#include <lve_frame_info.hpp>

#include <lve_model.hpp>

void LevelScene::update(float dt) 
{
    // Camera logic
    cameraController.moveInPlaneXZ(lve::LveEngine::instance->getGLFWWindow(), dt, viewerObject);
    // lve::LveEngine::instance->setCamera(lve::LveEngine::Camera{ translation = viewerObject.transform.translation, rotation = viewerObject.transform.rotation});
    camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

    // This is responsible for maintaining object projection size
    // across different window aspect ratios
    float aspect = lve::LveEngine::instance->getAspectRatio();
    // camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

    // Set up render items before frameInfo
    renderItems.clear();
    for (auto& [id, obj] : gameObjects) {
      if (!obj.model) continue;

      renderItems.push_back({obj.transform.mat4(), obj.transform.normalMatrix(), obj.model.get()});
    }
    lightItems.clear();
    for (auto& [id, obj] : gameObjects) {
      if (!obj.pointLight) continue;

      lve::LightRenderItem item;
      item.position = obj.transform.translation;
      item.color = obj.color;
      item.intensity = obj.pointLight->lightIntensity;
      item.radius = obj.transform.scale.x;

      auto offset = camera.getPosition() - item.position;
      item.distanceToCamera = glm::dot(offset, offset);

      lightItems.push_back(item);
    }

    // update camera on the UBO
    ubo.projection = camera.getProjection();
    ubo.view = camera.getView();
    ubo.inverseView = camera.getInverseView();
}

void LevelScene::loadModels() 
{
    // used to store the camera's state
    viewerObject.transform.translation.z = -2.5f;

    std::shared_ptr<lve::LveModel> lveModel = lve::LveModel::createModelFromFile("models/flat_vase.obj");

    auto gameObj = GameObject::createGameObject();
    gameObj.model = lveModel;
    gameObj.transform.translation = {-.5f, .5f, 0.f};
    gameObj.transform.scale = glm::vec3(3.f);
    gameObjects.emplace(gameObj.getId(), std::move(gameObj));

    lveModel = lve::LveModel::createModelFromFile("models/quad.obj");
    auto floor = GameObject::createGameObject();
    floor.model = lveModel;
    floor.transform.translation = {.0f, .5f, 0.f};
    floor.transform.scale = glm::vec3{3.f,1.f,3.f};
    gameObjects.emplace(floor.getId(), std::move(floor));
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
    auto pointLight = GameObject::makePointLight(0.2f);
    pointLight.color = lightColors[i];
    auto rotateLight = glm::rotate(
        glm::mat4(1.f),
        (i * glm::two_pi<float>()) / lightColors.size(),  // 360 degrees divided by 6
        {0.f, -1.f, 0.f});
    pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
    gameObjects.emplace(pointLight.getId(), std::move(pointLight));
  }
}
