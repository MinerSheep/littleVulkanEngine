
#include <lve_engine.hpp>
#include <scenes/servernavscene.hpp>
#include <lve_frame_info.hpp>

#include <lve_model.hpp>
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <string>

namespace
{
// Format an absolute sim time (in sim seconds) as a HH:MM:SS wall clock,
// wrapping at 24h. A negative time means "unknown" (idle / stranded vessel).
std::string formatSimClock(double simSeconds)
{
    if (simSeconds < 0.0)
        return "--:--:--";
    long total = static_cast<long>(simSeconds);
    long h = (total / 3600) % 24;
    long m = (total / 60) % 60;
    long s = total % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02ld:%02ld:%02ld", h, m, s);
    return std::string(buf);
}
} // namespace

// How often (in real seconds) to print the navigation readout.
static constexpr float kHudInterval = 0.5f;

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

    // Console HUD: per-vessel speed / distance / ETA, throttled to wall clock
    // so the terminal stays readable regardless of frame rate or simTimeStep.
    hudTimer += dt;
    if (hudTimer >= kHudInterval)
    {
        hudTimer = 0.f;
        for (const auto& vessel : nav.vessels)
        {
            std::cout << "V" << vessel.id
                      << std::fixed << std::setprecision(1)
                      << " | speed " << nav.vesselSpeedKnots(vessel) << " kn"
                      << " | dist " << nav.vesselDistanceNm(vessel) << " nm"
                      << " | ETA " << formatSimClock(nav.vesselEtaSimTime(vessel))
                      << " | timestep " << nav.simTimeStep
                      << '\n';
        }
    }

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

    // On-screen HUD text. The text renderer just appends solid dot-quads to
    // UIrenderItems, so it must go AFTER the markers above to paint on top of them.
    if (textRenderer)
    {
      std::string hud = "SERVER NAV\n";
      hud += "VESSELS " + std::to_string(nav.vessels.size()) +
             "  STATIONS " + std::to_string(nav.stations.size());
      if (!nav.vessels.empty())
      {
        const auto& v = nav.vessels.front();
        char line[64];
        std::snprintf(line, sizeof(line), "\nV%d SPD %.1fKN ETA %s",
                      static_cast<int>(v.id),
                      nav.vesselSpeedKnots(v),
                      formatSimClock(nav.vesselEtaSimTime(v)).c_str());
        hud += line;
      }

      const glm::vec2 origin{-0.97f, -0.95f};  // top-left in NDC (y is down)
      const float dotHeight = 0.012f;

      float aspect = lve::LveEngine::instance().getAspectRatio();
      if (aspect <= 0.f) aspect = 1.f;
      const float dotW = dotHeight / aspect;

      // Dark background panel so the text stays readable over the arrow field.
      lve::UIRenderItem panel{};
      panel.transform = glm::mat2(
          textRenderer->measureWidth(hud, dotHeight) + 2.f * dotW, 0.f,
          0.f, (lve::LveTextRenderer::lineCount(hud) * 8 - 1) * dotHeight + 2.f * dotHeight);
      panel.offset = {origin.x - dotW, origin.y - dotHeight};
      panel.color = {0.03f, 0.03f, 0.06f};
      panel.alpha = 1.f;
      panel.model = textRenderer->quad();
      UIrenderItems.push_back(panel);

      textRenderer->emit(UIrenderItems, hud, origin, dotHeight, {1.f, 1.f, 1.f});
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
    std::vector<lve::LveModel::Vertex> vertices{
      {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    {
      std::shared_ptr<lve::LveModel> lveModel = std::make_shared<lve::LveModel>(lve::LveEngine::instance().getDevice(), lve::LveModel::Builder{vertices, {0,1,2}});
      float windBaseline = USING_RTS ? nav.map[kGridSize/2][kGridSize/2].data.windSpeed * 2.0f : nav.map[kGridSize/2][kGridSize/2].weight * 2.0f;
      assert (windBaseline != 0 && "wind baseline is 0");
      for (int i = 0; i < kGridSize; i++)
      {
        for (int j = 0; j < kGridSize; j++)
        {
          WeatherCell cell = nav.map[i][j];
          auto ui = GameObject::createGameObject();
          ui.model = lveModel;
          float strength = USING_RTS ? cell.data.windSpeed : cell.weight;
          strength /= windBaseline;
          ui.color = {glm::mix(0.0f, 1.0f, strength), 0.0f, glm::mix(1.0f, 0.0f, strength)};
          ui.UI = true;
          
          // scaled from 0 to kGridSize which is 49.f
          glm::vec2 position = glm::vec2{i,j} / static_cast<float>(kGridSize - 1);
  
          RectTransformComponent* transform = ui.addComponent<RectTransformComponent>();
          transform->anchor = RectTransformComponent::UIAnchor::TopLeft;
          transform->translation = {position.x * 2.0f, position.y * 2.0f};
          transform->scale = glm::vec3(.025f);
          transform->rotation = cell.data.windDir;
          gameObjects.emplace(ui.getId(), std::move(ui));
        }
      }
    }

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

    // The Vulkan device now exists, so we can build the text renderer's quad model
    // BUILD TEXT RENDERER
    textRenderer = std::make_unique<lve::LveTextRenderer>(lve::LveEngine::instance().getDevice());
}

void ServerNavScene::setupLights() 
{
}

void ServerNavScene::cleanup() 
{ 
}
