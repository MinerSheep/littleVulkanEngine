#pragma once

#include <memory>
#include <string>
#include <vector>

namespace lve {


// uses miniaudio in lib folder
class LveAudio {
 public:
  static LveAudio& instance();

  LveAudio(const LveAudio&) = delete;
  LveAudio& operator=(const LveAudio&) = delete;

  bool init();
  void shutdown();
  bool ready() const;

  bool load(const std::string& name, const std::string& path);
  bool load(const std::string& name);
  int loadFolder(const std::string& folder);

  void play(const std::string& name);
  void play(const std::string& name, float volume, float pitch);

  // A bed that runs under a room for as long as you are in it
  //
  // Beds are streamed off the disk rather than decoded, because an ambience
  // recording runs for minutes and decoding one costs hundreds of megabytes.
  // They are registered by hand rather than swept up by loadFolder, so the size
  // of what is being opened is visible at the call site
  bool loadLoop(const std::string& name, const std::string& path);

  // Starts it going round, or leaves it be if it is already running
  // Safe to call every frame, which is how a room keeps its bed alive
  void loop(const std::string& name, float volume = 1.f);

  // Stops one clip, whether it is a bed or a one-shot
  void stop(const std::string& name);

  void stopAll();

  // TEMP diagnostic: prints where the mixer and one clip's voices have got to
  void traceClip(const std::string& name) const;

  void setMasterVolume(float volume);
  float getMasterVolume() const;

  void setSearchPath(const std::string& folder);
  std::vector<std::string> clipNames() const;

 private:
  LveAudio();
  ~LveAudio();

  struct Impl;
  std::unique_ptr<Impl> impl;
};

}  // namespace lve
