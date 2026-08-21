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
