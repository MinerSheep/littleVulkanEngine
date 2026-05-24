#include "first_app.hpp"

#include "keyboard_movement_controller.hpp"
#include "simple_render_system.hpp"
#include "lve_camera.hpp"
#include "lve_buffer.hpp"

#define MAX_FRAME_TIME 1.0f

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  // necessary for vulkan, glfw uses -1 to 1 depth by default
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <stdexcept>

#include <chrono>

namespace lve {

  struct GlobalUbo {
    glm::mat4 projectionView{1.f};
    glm::vec3 lightDirection = glm::normalize(glm::vec3{1.f,-3.f,-1.f});
  };

FirstApp::FirstApp() { loadGameObjects(); }

FirstApp::~FirstApp() {}

void FirstApp::run() {

  // this should create 2 instances, so for each frame, we can use the one thats not being rendered
  LveBuffer globalUboBuffer {
    lveDevice,
    sizeof(GlobalUbo),
    LveSwapChain::MAX_FRAMES_IN_FLIGHT,  // 2 - how many frames can be submit for rendering simultaneously
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, // host coherent is disabled for SELECTIVE FLUSHING
    lveDevice.properties.limits.minUniformBufferOffsetAlignment
  };

  globalUboBuffer.map();

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
      int frameIndex = lveRenderer.getFrameIndex();
      FrameInfo frameInfo{frameIndex, dt, commandBuffer, camera};

      // update
      GlobalUbo ubo{};
      ubo.projectionView = camera.getProjection() * camera.getView();
      globalUboBuffer.writeToIndex(&ubo, frameIndex);
      globalUboBuffer.flushIndex(frameIndex);

      // render
      // Being able to control when the render pass begins and ends is helpful for post processing
      // effects
      lveRenderer.beginSwapChainRenderPass(commandBuffer);
      simpleRenderSystem.renderGameObjects(frameInfo, gameObjects);

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



void FirstApp::loadGameObjects() {
  std::shared_ptr<LveModel> lveModel = LveModel::createModelFromFile(lveDevice, "models/flat_vase.obj");

  auto gameObj = LveGameObject::createGameObject();
  gameObj.model = lveModel;
  gameObj.transform.translation = {.0f, .5f, 2.5f};
  gameObj.transform.scale = glm::vec3(3.f);
  gameObjects.push_back(std::move(gameObj));
}
}  // namespace lve