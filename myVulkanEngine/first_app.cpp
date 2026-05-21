#include "first_app.hpp"

#include "keyboard_movement_controller.hpp"
#include "simple_render_system.hpp"
#include "lve_camera.hpp"

#define MAX_FRAME_TIME 1.0f

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  // necessary for vulkan, glfw uses -1 to 1 depth by default
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <stdexcept>

#include <chrono>

namespace lve {

FirstApp::FirstApp() { loadGameObjects(); }

FirstApp::~FirstApp() {}

void FirstApp::run() {
  SimpleRenderSystem simpleRenderSystem{lveDevice, lveRenderer.getSwapChainRenderPass()};
  LveCamera camera{};
  //camera.setViewDirection(glm::vec3{0.f}, glm::vec3(0.5f,0.f,1.f));

  // this is directly associated with setPerspectiveProjection btw, 10.f is far plane, if we go beyond that for position, we leave bounds
  // camera.setViewTarget(glm::vec3(-1.f,-2.f,2.f), glm::vec3(0.f,0.f,2.5f));

  // used to store the camera's state
  auto viewerObject = LveGameObject::createGameObject();
  KeyboardMovementController cameraController{};

  auto currentTime = std::chrono::high_resolution_clock::now();
  
  while (!lveWindow.shouldClose()) {
    // this causes glitchiness on ubuntu because it blocks
    glfwPollEvents();

    // Take the time after the block
    auto newTime = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
    currentTime = newTime;

    dt = glm::min(dt, MAX_FRAME_TIME);

    cameraController.moveInPlaneXZ(lveWindow.getGLFWWindow(), dt, viewerObject);
    camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

    // This is responsible for maintaining object projection size
    // across different window aspect ratios
    float aspect = lveRenderer.getAspectRatio();
    // camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 10.f);

    // Returns null if swap chain needs to be recreated!
    if (auto commandBuffer = lveRenderer.beginFrame()) {
      // Being able to control when the render pass begins and ends is helpful for post processing
      // effects
      lveRenderer.beginSwapChainRenderPass(commandBuffer);
      simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects, camera);

      lveRenderer.endSwapChainRenderPass(commandBuffer);
      lveRenderer.endFrame();
    }
  }

  // If you get many messages on close, it's likely because a command buffer was still being
  // executed This is the fix
  vkDeviceWaitIdle(lveDevice.device());
}
// void sierpinski(
//     std::vector<LveModel::Vertex>& vertices,
//     int depth,
//     glm::vec2 left,
//     glm::vec2 right,
//     glm::vec2 top) {
//   if (depth <= 0) {
//     vertices.push_back({top, {1.0f, 0.0f, 0.0f}});
//     vertices.push_back({right, {1.0f, 0.0f, 0.0f}});
//     vertices.push_back({left, {1.0f, 0.0f, 0.0f}});
//   } else {
//     auto leftTop = 0.5f * (left + top);
//     auto rightTop = 0.5f * (right + top);
//     auto leftRight = 0.5f * (left + right);
//     sierpinski(vertices, depth - 1, left, leftRight, leftTop);
//     sierpinski(vertices, depth - 1, leftRight, right, rightTop);
//     sierpinski(vertices, depth - 1, leftTop, rightTop, top);
//   }
// }

// temporary helper function, creates a 1x1x1 cube centered at offset
std::unique_ptr<LveModel> createCubeModel(LveDevice& device, glm::vec3 offset) {
  LveModel::Builder modelBuilder{};
  modelBuilder.vertices = {

      // left face (white)
      {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
      {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
      {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},
      {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
      {{-.5f, .5f, -.5f}, {.9f, .9f, .9f}},
      {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},

      // right face (yellow)
      {{.5f, -.5f, -.5f}, {.8f, .8f, .1f}},
      {{.5f, .5f, .5f}, {.8f, .8f, .1f}},
      {{.5f, -.5f, .5f}, {.8f, .8f, .1f}},
      {{.5f, -.5f, -.5f}, {.8f, .8f, .1f}},
      {{.5f, .5f, -.5f}, {.8f, .8f, .1f}},
      {{.5f, .5f, .5f}, {.8f, .8f, .1f}},

      // top face (orange, remember y axis points down)
      {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
      {{.5f, -.5f, .5f}, {.9f, .6f, .1f}},
      {{-.5f, -.5f, .5f}, {.9f, .6f, .1f}},
      {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
      {{.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
      {{.5f, -.5f, .5f}, {.9f, .6f, .1f}},

      // bottom face (red)
      {{-.5f, .5f, -.5f}, {.8f, .1f, .1f}},
      {{.5f, .5f, .5f}, {.8f, .1f, .1f}},
      {{-.5f, .5f, .5f}, {.8f, .1f, .1f}},
      {{-.5f, .5f, -.5f}, {.8f, .1f, .1f}},
      {{.5f, .5f, -.5f}, {.8f, .1f, .1f}},
      {{.5f, .5f, .5f}, {.8f, .1f, .1f}},

      // nose face (blue)
      {{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
      {{.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
      {{-.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
      {{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
      {{.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
      {{.5f, .5f, 0.5f}, {.1f, .1f, .8f}},

      // tail face (green)
      {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
      {{.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
      {{-.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
      {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
      {{.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
      {{.5f, .5f, -0.5f}, {.1f, .8f, .1f}},

  };
  for (auto& v : modelBuilder.vertices) {
    v.position += offset;
  }
  return std::make_unique<LveModel>(device, modelBuilder);
}

void FirstApp::loadGameObjects() {
  std::shared_ptr<LveModel> lveModel = createCubeModel(lveDevice, {.0f, .0f, .0f});

  auto cube = LveGameObject::createGameObject();
  cube.model = lveModel;
  cube.transform.translation = {.0f, .0f, 2.5f};
  cube.transform.scale = {.5f, .5f, .5f};
  gameObjects.push_back(std::move(cube));
}
}  // namespace lve