#pragma once

#include "lve_game_object.hpp"  // Component, GameObject

#include <glm/glm.hpp>

// Placement for a flat 2D thing drawn on top of the world, like a HUD label or a map pin
//
// A world object uses TransformComponent and lives in 3D
// A UI object uses this instead and lives in screen space (Vulkan NDC, -1 to +1 on both axes)
//
// The anchor picks which corner or edge of the screen the object hangs off
// translation is then an offset from that anchor, so a pin stuck to the top-left
// stays in the top-left when the window is resized
struct RectTransformComponent : public Component {
  enum class UIAnchor { Center, TopLeft, Top, TopRight, Left, Right, BottomLeft, Bottom, BottomRight };

  UIAnchor anchor = UIAnchor::Center;
  glm::vec2 translation{};  // offset from the anchor
  glm::vec2 scale{1.f, 1.f};
  float rotation{};

  // Rotation and scale squashed into one 2x2, which is what UIRenderItem wants
  glm::mat2 mat2() {
    float s = glm::sin(rotation);
    float c = glm::cos(rotation);
    glm::mat2 rotMatrix{{c, s}, {-s, c}};

    glm::mat2 scaleMat{{scale.x, .0f}, {.0f, scale.y}};
    return rotMatrix * scaleMat;
  }

  // Where the chosen anchor sits in NDC
  // Remember +y is DOWN in Vulkan NDC, so the top of the screen is -1
  inline glm::vec2 anchorNdc(UIAnchor a) {
    switch (a) {
      case UIAnchor::TopLeft:     return {-1,-1}; case UIAnchor::Top:    return { 0,-1};
      case UIAnchor::TopRight:    return { 1,-1}; case UIAnchor::Left:   return {-1, 0};
      case UIAnchor::Center:      return { 0, 0}; case UIAnchor::Right:  return { 1, 0};
      case UIAnchor::BottomLeft:  return {-1, 1}; case UIAnchor::Bottom: return { 0, 1};
      case UIAnchor::BottomRight: return { 1, 1};
    }
    return {0,0};
  }
};
