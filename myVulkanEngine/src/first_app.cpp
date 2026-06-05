#include "first_app.hpp"

#include "keyboard_movement_controller.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/point_light_system.hpp"
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

FirstApp::FirstApp() {
  // since the fns return a reference, we can chain initialization here
  globalPool = LveDescriptorPool::Builder(lveDevice)
    // we can create 2 SETS
    .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)       
    // we can create 2 UNIFORM BUFFER DESCRIPTORS to store in sets
    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT) 
    .build(); 

  loadGameObjects();
 }

FirstApp::~FirstApp() {}

void FirstApp::run() {

  const int globalUniformBufferSize = LveSwapChain::MAX_FRAMES_IN_FLIGHT;

  // this should create 2 instances, so for each frame, we can use the one thats not being rendered
  LveBuffer globalUboBuffer {
    lveDevice,
    sizeof(GlobalUbo),
    globalUniformBufferSize,  // 2 - how many frames can be submit for rendering simultaneously
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, // host coherent is disabled for SELECTIVE FLUSHING
    lveDevice.properties.limits.minUniformBufferOffsetAlignment
  };

  globalUboBuffer.map();

  auto globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
    .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
    // .addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
    .build();

  // this will be destroyed when run ends
  std::vector<VkDescriptorSet> globalDescriptorSets(globalUniformBufferSize);

  // this is currently taking 2 uniform buffers, I only have one...
  for (int i = 0; i < globalUniformBufferSize; i++)
  {
    auto bufferInfo = globalUboBuffer.descriptorInfoForIndex(i);

    // descriptor writer class handles moving uniform buffer info INTO the descriptor set
    LveDescriptorWriter(*globalSetLayout, *globalPool)
      .writeBuffer(0, &bufferInfo)
      .build(globalDescriptorSets[i]);
  }

  SimpleRenderSystem simpleRenderSystem{lveDevice, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
  PointLightSystem pointLightSystem{lveDevice, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
  LveCamera camera{};
  //camera.setViewDirection(glm::vec3{0.f}, glm::vec3(0.5f,0.f,1.f));

  // this is directly associated with setPerspectiveProjection btw, 10.f is far plane, if we go beyond that for position, we leave bounds
  // camera.setViewTarget(glm::vec3(-1.f,-2.f,2.f), glm::vec3(0.f,0.f,2.5f));

  // used to store the camera's state
  auto viewerObject = LveGameObject::createGameObject();
  viewerObject.transform.translation.z = -2.5f;
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
    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

    // Returns null if swap chain needs to be recreated!
    if (auto commandBuffer = lveRenderer.beginFrame()) {
      int frameIndex = lveRenderer.getFrameIndex();
      FrameInfo frameInfo{frameIndex, dt, commandBuffer, camera, globalDescriptorSets[frameIndex], gameObjects};

      // update
      GlobalUbo ubo{};
      ubo.projection = camera.getProjection();
      ubo.view = camera.getView();
      globalUboBuffer.writeToIndex(&ubo, frameIndex);
      globalUboBuffer.flushIndex(frameIndex);

      // render
      // Being able to control when the render pass begins and ends is helpful for post processing
      // effects
      lveRenderer.beginSwapChainRenderPass(commandBuffer);
      simpleRenderSystem.renderGameObjects(frameInfo);
      pointLightSystem.render(frameInfo);

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
  gameObj.transform.translation = {-.5f, .5f, 0.f};
  gameObj.transform.scale = glm::vec3(3.f);
  gameObjects.emplace(gameObj.getId(), std::move(gameObj));

  lveModel = LveModel::createModelFromFile(lveDevice, "models/quad.obj");
  auto floor = LveGameObject::createGameObject();
  floor.model = lveModel;
  floor.transform.translation = {.0f, .5f, 0.f};
  floor.transform.scale = glm::vec3{3.f,1.f,3.f};
  gameObjects.emplace(floor.getId(), std::move(floor));
}
}  // namespace lve