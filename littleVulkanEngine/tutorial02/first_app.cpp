#include "first_app.hpp"

#include "simple_render_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  // necessary for vulkan, glfw uses -1 to 1 depth by default
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <stdexcept>

namespace lve {

FirstApp::FirstApp() { loadGameObjects(); }

FirstApp::~FirstApp() {}

void FirstApp::run() {
  SimpleRenderSystem simpleRenderSystem{lveDevice, lveRenderer.getSwapChainRenderPass()};

  while (!lveWindow.shouldClose()) {
    // this causes glitchiness on ubuntu because it blocks
    glfwPollEvents();

    // Returns null if swap chain needs to be recreated!
    if (auto commandBuffer = lveRenderer.beginFrame()) {
      // Being able to control when the render pass begins and ends is helpful for post processing
      // effects
      lveRenderer.beginSwapChainRenderPass(commandBuffer);
      simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects);

      lveRenderer.endSwapChainRenderPass(commandBuffer);
      lveRenderer.endFrame();
    }
  }

  // If you get many messages on close, it's likely because a command buffer was still being
  // executed This is the fix
  vkDeviceWaitIdle(lveDevice.device());
}
void sierpinski(
    std::vector<LveModel::Vertex>& vertices,
    int depth,
    glm::vec2 left,
    glm::vec2 right,
    glm::vec2 top) {
  if (depth <= 0) {
    vertices.push_back({top, {1.0f, 0.0f, 0.0f}});
    vertices.push_back({right, {1.0f, 0.0f, 0.0f}});
    vertices.push_back({left, {1.0f, 0.0f, 0.0f}});
  } else {
    auto leftTop = 0.5f * (left + top);
    auto rightTop = 0.5f * (right + top);
    auto leftRight = 0.5f * (left + right);
    sierpinski(vertices, depth - 1, left, leftRight, leftTop);
    sierpinski(vertices, depth - 1, leftRight, right, rightTop);
    sierpinski(vertices, depth - 1, leftTop, rightTop, top);
  }
}
void FirstApp::loadGameObjects() {
  std::vector<LveModel::Vertex> vertices{
      {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

  // sierpinski(vertices, 5, {-0.5f, 0.5f}, {0.5f, 0.5f}, {0.0f, -0.5f});

  auto lveModel = std::make_shared<LveModel>(lveDevice, vertices);

  auto triangle = LveGameObject::createGameObject();
  triangle.model = lveModel;
  triangle.color = {.1f, .8f, .1f};
  triangle.transform2d.translation.x = .2f;
  triangle.transform2d.scale = {2.f, .5f};
  // we are using radians.  So 2pi is 360 degrees.  We are taking .25 of that which is 90 degrees
  // clockwise.
  triangle.transform2d.rotation = .25f * glm::two_pi<float>();

  gameObjects.push_back(std::move(triangle));
}
}  // namespace lve