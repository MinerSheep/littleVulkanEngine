
#pragma once

#include "lve_frame_info.hpp"

namespace lve
{
    class LveScene {
    public:
        LveScene() = default;
        virtual ~LveScene() = default;
        virtual void update(float dt) {}
        virtual void cleanup() {}

        virtual void loadModels() {}
        virtual void setupLights() {}

        GlobalUbo ubo{};
        std::vector<RenderItem> renderItems;
        std::vector<LightRenderItem> lightItems;
    };
} // namespace lve