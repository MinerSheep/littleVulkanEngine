
#pragma once

#include "lve_frame_info.hpp"
#include "lve_event.hpp"

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

        // Called by the game loop (via EventDispatcher) for each event delivered
        // this frame. Override in a subclass to react to input or game events;
        // the default does nothing so scenes only handle what they care about
        virtual void onEvent(const Event& event) {}

        GlobalUbo ubo{};
        std::vector<RenderItem> renderItems;
        std::vector<UIRenderItem> UIrenderItems;
        std::vector<LightRenderItem> lightItems;
        std::vector<SkinnedRenderItem> skinnedRenderItems;

        // Flat quads painted first, so the scene is drawn on top of them
        // Use for a stylish background rather than pitch black
        std::vector<UIRenderItem> backgroundItems;
    };
} // namespace lve