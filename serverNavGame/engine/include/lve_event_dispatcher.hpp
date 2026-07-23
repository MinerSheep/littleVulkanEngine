#pragma once

#include "lve_event.hpp"

// std
#include <functional>
#include <vector>

namespace lve {

  // A tiny publish/subscribe hub. Producers `post` events (queued) or `emit`
  // them (delivered right away); consumers `subscribe` a callback. Once per
  // frame the game loop calls `dispatch()` to drain the queue to subscribers.
  //
  // Its an engine-wide singleton; Not thread-safe: keep post/dispatch on the loop thread
  class EventDispatcher {
   public:

    // listener is how others subscribe by using lambda and event as a parameter
    // event is from std::function, event.i catches button presses for example
    using Listener = std::function<void(const Event&)>;

    static EventDispatcher& instance() {
      static EventDispatcher inst;
      return inst;
    }

    // Register a callback and get back an id you can later `unsubscribe`. Pass
    // EventType::None (the default) to hear *every* event, or a specific type
    // to only be called for that kind. Listeners fire in subscription order
    int subscribe(Listener cb, EventType filter = EventType::None) {
      subs.push_back({nextId, filter, std::move(cb)});
      return nextId++;
    }
    void unsubscribe(int id) {
      for (auto it = subs.begin(); it != subs.end(); ++it)
        if (it->id == id) { subs.erase(it); return; }
    }

    // Queue an event for delivery on the next `dispatch()`. This is the usual
    // path: it keeps all handling at one well-defined point in the game loop
    void post(const Event& e) { queue.push_back(e); }
    void post(Event&& e) { queue.push_back(std::move(e)); }

    // Deliver an event to matching subscribers immediately, skipping the queue.
    // Handy for events that must be acted on before the next dispatch point
    void emit(const Event& e) {
      for (auto& s : subs)
        if (s.filter == EventType::None || s.filter == e.type)
          s.cb(e);
    }

    // Drain the queued events to their subscribers. Call once per frame from
    // the game loop, before the scene's update()
    void dispatch() {
      // Swap the queue out first, so a handler that posts more events doesn't
      // grow the container we're iterating — those land in next frame's queue
      pending.swap(queue);
      for (auto& e : pending) emit(e);
      pending.clear();
    }

    // Drop anything queued but not yet dispatched (e.g. on a scene teardown)
    void clear() { queue.clear(); }

   private:
    struct Sub {
      int id;
      EventType filter;
      Listener cb;
    };

    std::vector<Sub> subs;
    std::vector<Event> queue;
    std::vector<Event> pending;  // reused scratch buffer for dispatch()
    int nextId = 1;
  };

}  // namespace lve
