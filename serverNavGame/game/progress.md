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



June 30th
I've refactored way too many things.  All engine mechanics should be in the engine section and all game mechanics should be in the game section.  But theres a small problem with the free ordering.  I've tried setting things up so everything is created and freed in the same order by calling them in the right order from main, HOWEVER, there is still an issue.  Whenever I close by clicking the window X, it gives me a message saying error OBJ for the descriptor pool and descriptor set not being destroyed before the device.

This shouldn't happen at all, because I intentionally made the Descriptor Pool (which also stores the sets) into a unique ptr and it is located below the device in LveEngine.
Since deconstruction happens from bottom to top, the descriptor pool and its corresponding sets SHOULD just destroy before the device in every case.  Au contraire.

I decided to continue with making the game aspects.  I've already gotten it running like it was before with the exception of moving lights.  Thats because there's not enough abstraction when it comes to components, so I'm ripping out the pointLight and transform components for this reason.
There have been a few linker errors, I learned that template functions should not exist in cpp files for example.

Right now I'm running into an issue with making the gameObjects and adding them to the map.  For some reason it seg faults and not even AI knows why.