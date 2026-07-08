June 26th
Currently working on refactoring the LveEngine to separate its frontend components from its backend components
This is so I can use it as an engine to actually make a game, rather than a one off project

There are a lot of front end pieces thrown into the mix that need to be rearranged, mainly the game object and movement support
It throws a wrench in rendering.  I'm revamping it now so that the render system only takes render data and has no idea about game objects at all.  It causes another problem with point lights, how do those work if they are game objects, but must also be rendered as lights?

// we need to sort lights by distance to camera
  std::map<float, LveGameObject::id_t> sorted;
  for (auto& kv : frameInfo.gameObjects)
    {
      auto& obj = kv.second;
      // no point light component = not point light
      if (obj.pointLight == nullptr) continue;

      // calc distance w length squared
      auto offset = frameInfo.camera.getPosition() - obj.transform.translation;
      float disSquared = glm::dot(offset, offset);
      sorted[disSquared] = obj.getId();
    }

  auto rotateLight = glm::rotate(glm::mat4(1.f), 0.5f * frameInfo.frameTime, {0.f, -1.f, 0.f});


    // update light position per frame!
    obj.transform.translation = glm::vec3(rotateLight * glm::vec4(obj.transform.translation, 1.f));

July 8th

The issue I've been running into for so long is that the game file was recompiling without the engine recompiling.  It led to a size mismatch between the base class LveScene and the derived class LevelScene - data corruption.  Now I'm working on actually setting up the sim and also cleaning out the asan errors thanks to Claude Code's help.

Audit Command to enable within game folder
ASAN_OPTIONS=fast_unwind_on_malloc=0 \
LSAN_OPTIONS="suppressions=$PWD/lsan.supp:print_suppressions=1" \
./game