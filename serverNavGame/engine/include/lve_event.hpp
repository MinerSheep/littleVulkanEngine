#pragma once

// lib
#include <glm/glm.hpp>

#include <string>

namespace lve {

  // Broad category of a dispatched event. Extend this freely — the dispatcher
  // and the scenes only ever switch/compare on this tag
  enum class EventType {
    None = 0,
    KeyPressed,
    KeyReleased,
    SceneSwitch,
    WindowResized,
    Custom,        // disambiguate your own kinds with the `name` field
  };

  // A single event flowing through the EventDispatcher. It's a plain tagged
  // struct (like RenderItem) rather than a class hierarchy: one payload wide
  // enough for the common cases — a key code, a scalar, a 2D vector, a name —
  // so producers fill only what they need and consumers read what they expect
  struct Event {
    EventType type = EventType::None;

    int         i = 0;      // key code, scene index, entity id, ...
    float       f = 0.f;    // a scalar payload (delta, amount, ...)
    glm::vec2   v{0.f};     // a 2D payload (mouse pos, resize extent, ...)
    std::string name;       // free-form tag, mainly for EventType::Custom

    // A consumer may set this so later listeners can tell it was already dealt
    // with. The dispatcher doesn't act on it — it's a convention for handlers
    bool handled = false;

    Event() = default;
    explicit Event(EventType t) : type(t) {}
  };

  // Convenience makers so call sites read nicely:
  //   dispatcher.post(lve::makeKeyEvent(GLFW_KEY_1));
  inline Event makeKeyEvent(int key, bool pressed = true) {
    Event e{pressed ? EventType::KeyPressed : EventType::KeyReleased};
    e.i = key;
    return e;
  }
  inline Event makeSceneSwitch(int index) {
    Event e{EventType::SceneSwitch};
    e.i = index;
    return e;
  }
  inline Event makeCustom(std::string name, float value = 0.f) {
    Event e{EventType::Custom};
    e.name = std::move(name);
    e.f = value;
    return e;
  }

}  // namespace lve
