#pragma once

#include "petscop/dialog_box.hpp"
#include "petscop/prop.hpp"

#include <cstddef>
#include <vector>

struct ColliderComponent;
struct KeyboardMovementComponent;

namespace petscop {

// How far through a prop's list of actions the press has got
struct Script {
  bool running = false;
  int prop = -1;
  std::size_t next = 0;
};

// Reads E, finds the nearest prop in reach, and runs its list of actions
// The room owns the props and the dialog box, bind() points at them
class InteractionRunner {
 public:
  void bind(std::vector<Prop>* props, DialogBox* dialog);

  // Drops whatever was running, for a room swap or a shutdown
  void reset();

  void update(ColliderComponent* playerCollider, KeyboardMovementComponent* playerMover);

  // Which prop the floating E is drawn over, or -1 for none
  int nearProp() const { return nearest; }

  bool busy() const { return script.running; }

  float range = 1.2f;

 private:
  // Returns true when the action wants reading before the next one starts
  bool applyAction(const MapAction& action, int owner, bool backwards);

  // Steps through the list until an action blocks or it runs out
  void runScript();

  std::vector<Prop>* props = nullptr;
  DialogBox* dialog = nullptr;

  Script script;
  int nearest = -1;
  bool actionPrevDown = false;
};

}  // namespace petscop
