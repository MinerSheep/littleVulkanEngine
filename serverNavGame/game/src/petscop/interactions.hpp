#pragma once

#include "petscop/dialog_box.hpp"
#include "petscop/prop.hpp"

#include <cstddef>
#include <vector>

struct ColliderComponent;
struct KeyboardMovementComponent;

namespace petscop {

struct GameState;

// How far through a prop's list of actions the press has got
struct Script {
  bool running = false;
  int prop = -1;
  std::size_t next = 0;

  // A take that found nothing in his pockets, which ends the list there
  bool stopped = false;
};

// Reads E, finds the nearest prop in reach, and runs its list of actions
// The room owns the props, the dialog box and the save, bind() points at them
class InteractionRunner {
 public:
  void bind(std::vector<Prop>* props, DialogBox* dialog, GameState* state);

  // Drops whatever was running, for a room swap or a shutdown
  void reset();

  void update(ColliderComponent* playerCollider, KeyboardMovementComponent* playerMover);

  // Which prop the floating E is drawn over, or -1 for none
  int nearProp() const { return nearest; }

  // Which prop a press just set going, or -1 when none did this frame
  int startedProp() const { return started; }

  bool busy() const { return script.running; }

  float range = 1.2f;

 private:
  // Returns true when the action wants reading before the next one starts
  bool applyAction(const MapAction& action, int owner, bool backwards);

  // Whether any of the prop's lines pass their 'when'
  bool hasSomethingToDo(const Prop& prop) const;

  // Steps through the list until an action blocks or it runs out
  void runScript();

  std::vector<Prop>* props = nullptr;
  DialogBox* dialog = nullptr;
  GameState* state = nullptr;

  Script script;
  int nearest = -1;
  int started = -1;
  bool actionPrevDown = false;
};

}  // namespace petscop
