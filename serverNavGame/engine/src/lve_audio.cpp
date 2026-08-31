#include "lve_audio.hpp"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#include <miniaudio/miniaudio.h>

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <unordered_map>

namespace lve {

namespace {

// max amount of sounds that can be playing
constexpr std::size_t MAX_VOICES = 8;
constexpr ma_uint32 CLIP_FLAGS = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;

// A bed is read off the disk as it plays. Decoding one would mean holding the
// whole recording in memory, and these run for minutes
constexpr ma_uint32 BED_FLAGS = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;
const char* const EXTENSIONS[] = {".wav", ".ogg", ".mp3", ".flac"};

// This is a necessary check before failing
// Basically, miniaudio crashes if you try to use an audio 
bool fileExists(const std::string& path) {
  struct stat info;
  return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

}  // namespace

struct LveAudio::Impl {
  struct Clip {
    std::vector<std::unique_ptr<ma_sound>> voices;
  };

  ma_engine engine{};
  bool started = false;
  bool tried = false;
  float masterVolume = 1.f;
  std::string searchPath = "sounds";
  std::unordered_map<std::string, Clip> clips;

  // One voice each, kept apart from the one-shots because they are streamed
  std::unordered_map<std::string, std::unique_ptr<ma_sound>> beds;

  ma_sound* freeVoice(Clip& clip) {
    for (std::unique_ptr<ma_sound>& voice : clip.voices) {
      if (!ma_sound_is_playing(voice.get())) return voice.get();
    }

    if (clip.voices.size() >= MAX_VOICES) return clip.voices.front().get();

    // sound_init_copy is used here so audio is only decoded once
    // when detected again, it reuses the sample
    auto copy = std::unique_ptr<ma_sound>(new ma_sound());
    if (ma_sound_init_copy(&engine, clip.voices.front().get(), CLIP_FLAGS, nullptr, copy.get()) !=
        MA_SUCCESS) {
      return clip.voices.front().get();
    }

    clip.voices.push_back(std::move(copy));
    return clip.voices.back().get();
  }
};

LveAudio::LveAudio() : impl(new Impl()) {}

LveAudio::~LveAudio() { shutdown(); }

LveAudio& LveAudio::instance() {
  static LveAudio inst;
  return inst;
}

bool LveAudio::init() {
  if (impl->tried) return impl->started;
  impl->tried = true;

  ma_engine_config config = ma_engine_config_init();
  if (ma_engine_init(&config, &impl->engine) != MA_SUCCESS) {
    std::cout << "[audio] no sound device, the game runs silently" << std::endl;
    return false;
  }

  impl->started = true;
  ma_engine_set_volume(&impl->engine, impl->masterVolume);

  const char* backend = "unknown";
  ma_device* device = ma_engine_get_device(&impl->engine);
  if (device && device->pContext) backend = ma_get_backend_name(device->pContext->backend);

  if (device && device->pContext && device->pContext->backend == ma_backend_null) {
    std::cout << "[audio] no sound device, running on the null backend so nothing will be heard"
              << std::endl;
  } else {
    std::cout << "[audio] ready on " << backend << std::endl;
  }
  return true;
}

void LveAudio::shutdown() {
  if (!impl->started) {
    impl->clips.clear();
    return;
  }

  for (auto& entry : impl->clips) {
    for (std::unique_ptr<ma_sound>& voice : entry.second.voices) ma_sound_uninit(voice.get());
  }
  impl->clips.clear();

  for (auto& entry : impl->beds) ma_sound_uninit(entry.second.get());
  impl->beds.clear();

  ma_engine_uninit(&impl->engine);
  impl->started = false;
}

bool LveAudio::ready() const { return impl->started; }

void LveAudio::setSearchPath(const std::string& folder) { impl->searchPath = folder; }

bool LveAudio::load(const std::string& name, const std::string& path) {
  if (!impl->tried) init();
  if (!impl->started) return false;
  if (impl->clips.count(name)) return true;

  if (!fileExists(path)) return false;

  auto first = std::unique_ptr<ma_sound>(new ma_sound());
  if (ma_sound_init_from_file(&impl->engine, path.c_str(), CLIP_FLAGS, nullptr, nullptr,
                              first.get()) != MA_SUCCESS) {
    return false;
  }

  Impl::Clip clip;
  clip.voices.push_back(std::move(first));
  impl->clips.emplace(name, std::move(clip));
  return true;
}

bool LveAudio::load(const std::string& name) {
  if (name.find('.') != std::string::npos || name.find('/') != std::string::npos) {
    return load(name, name);
  }

  for (const char* extension : EXTENSIONS) {
    const std::string path = impl->searchPath + "/" + name + extension;

    if (fileExists(path)) 
      return load(name, path);
  }
  return false;
}

int LveAudio::loadFolder(const std::string& folder) {
  DIR* directory = opendir(folder.c_str());
  if (!directory) {

    std::cout << "[audio] no folder called '" << folder << "'" << std::endl;
    return 0;
  }

  int loaded = 0;
  while (dirent* entry = readdir(directory)) {

    const std::string file = entry->d_name;
    const std::size_t dot = file.rfind('.');
    if (dot == std::string::npos || dot == 0) 
      continue;

    std::string extension = file.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    bool known = false;
    for (const char* candidate : EXTENSIONS) known = known || extension == candidate;

    if (!known) 
      continue;

    if (load(file.substr(0, dot), folder + "/" + file)) loaded++;
  }

  closedir(directory);
  return loaded;
}

void LveAudio::play(const std::string& name) { play(name, 1.f, 1.f); }

void LveAudio::play(const std::string& name, float volume, float pitch) {
  if (!impl->tried) init();
  if (!impl->started || name.empty()) 
    return;

  auto found = impl->clips.find(name);
  if (found == impl->clips.end()) {
    if (!load(name)) {
      std::cout << "[audio] no clip called '" << name << "'" << std::endl;
      impl->clips.emplace(name, Impl::Clip{});

      return;
    }
    found = impl->clips.find(name);
  }

  if (found->second.voices.empty()) 
    return;

  ma_sound* voice = impl->freeVoice(found->second);
  if (!voice) 
    return;

  ma_sound_stop(voice);
  ma_sound_seek_to_pcm_frame(voice, 0);
  ma_sound_set_volume(voice, std::max(0.f, volume));
  ma_sound_set_pitch(voice, pitch > 0.f ? pitch : 1.f);
  ma_sound_start(voice);
}

bool LveAudio::loadLoop(const std::string& name, const std::string& path) {
  if (!impl->tried) init();
  if (!impl->started) return false;
  if (impl->beds.count(name)) return true;

  if (!fileExists(path)) {
    std::cout << "[audio] no loop at '" << path << "'" << std::endl;
    return false;
  }

  auto bed = std::unique_ptr<ma_sound>(new ma_sound());
  if (ma_sound_init_from_file(&impl->engine, path.c_str(), BED_FLAGS, nullptr, nullptr,
                              bed.get()) != MA_SUCCESS) {
    std::cout << "[audio] could not open the loop at '" << path << "'" << std::endl;
    return false;
  }

  ma_sound_set_looping(bed.get(), MA_TRUE);
  impl->beds.emplace(name, std::move(bed));
  return true;
}

void LveAudio::loop(const std::string& name, float volume) {
  if (!impl->started || name.empty()) return;

  std::unordered_map<std::string, std::unique_ptr<ma_sound>>::iterator found =
      impl->beds.find(name);
  if (found == impl->beds.end()) return;

  ma_sound* bed = found->second.get();
  ma_sound_set_volume(bed, std::max(0.f, volume));

  // Already going round. Starting it again would drag it back to the top
  if (ma_sound_is_playing(bed)) return;

  ma_sound_seek_to_pcm_frame(bed, 0);
  ma_sound_start(bed);
}

void LveAudio::stop(const std::string& name) {
  if (!impl->started) return;

  std::unordered_map<std::string, std::unique_ptr<ma_sound>>::iterator bed =
      impl->beds.find(name);
  if (bed != impl->beds.end()) ma_sound_stop(bed->second.get());

  std::unordered_map<std::string, Impl::Clip>::iterator clip = impl->clips.find(name);
  if (clip == impl->clips.end()) return;
  for (std::unique_ptr<ma_sound>& voice : clip->second.voices) ma_sound_stop(voice.get());
}

// Everything at once, beds included. A room puts its own bed back the next frame
void LveAudio::stopAll() {
  if (!impl->started) return;
  for (auto& entry : impl->clips) {
    for (std::unique_ptr<ma_sound>& voice : entry.second.voices) ma_sound_stop(voice.get());
  }
  for (auto& entry : impl->beds) ma_sound_stop(entry.second.get());
}

// TEMP diagnostic: one line a frame, mixer clock against the wall clock
void LveAudio::traceClip(const std::string& name) const {
  if (!impl->started) return;

  std::unordered_map<std::string, Impl::Clip>::const_iterator found = impl->clips.find(name);
  if (found == impl->clips.end()) return;

  static const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  const double wall =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  const double rate = static_cast<double>(ma_engine_get_sample_rate(&impl->engine));
  const double mixer = 1000.0 * ma_engine_get_time_in_pcm_frames(&impl->engine) / rate;

  std::cout << "[audiotrace] wall " << static_cast<long>(wall) << " mixer "
            << static_cast<long>(mixer) << " lag " << static_cast<long>(wall - mixer);
  for (std::size_t i = 0; i < found->second.voices.size(); i++) {
    ma_uint64 cursor = 0;
    ma_sound_get_cursor_in_pcm_frames(found->second.voices[i].get(), &cursor);
    std::cout << "  v" << i << " " << static_cast<long>(1000.0 * cursor / rate)
              << (ma_sound_is_playing(found->second.voices[i].get()) ? "*" : " ");
  }
  std::cout << std::endl;
}

void LveAudio::setMasterVolume(float volume) {
  impl->masterVolume = std::max(0.f, volume);
  if (impl->started) ma_engine_set_volume(&impl->engine, impl->masterVolume);
}

float LveAudio::getMasterVolume() const { return impl->masterVolume; }

std::vector<std::string> LveAudio::clipNames() const {
  std::vector<std::string> names;
  names.reserve(impl->clips.size());
  for (const auto& entry : impl->clips) names.push_back(entry.first);
  std::sort(names.begin(), names.end());
  return names;
}

}  // namespace lve
