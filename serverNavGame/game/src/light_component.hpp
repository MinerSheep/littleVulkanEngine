#pragma once

#include "lve_game_object.hpp"  // Component, GameObject

// Marks a GameObject as a point light
// Pairs with a sibling TransformComponent, which says where the light is
//
// The scene walks its objects each frame and turns every one of these into a
// LightRenderItem, so the light only exists as far as the scene bothers to collect it
struct PointLightComponent : public Component
{
  float lightIntensity = 1.0f;
};
