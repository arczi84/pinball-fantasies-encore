#pragma once
#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include "core/Mutex.h"
#include <vector>

namespace encore {

/// Pulls stereo float audio from whichever source is current into an SDL3 audio stream.
class AudioDevice {
 public:
  /// The most frames made at a time (the buffers are made for this many once, in open).
  static constexpr std::size_t kChunk = 4096;
  using Source = std::function<void(float* out, int frames)>;

  AudioDevice() = default;
  ~AudioDevice();
  AudioDevice(const AudioDevice&) = delete;
  AudioDevice& operator=(const AudioDevice&) = delete;

  bool open(int sampleRate);
  void close();
  /// Replaces the source; safe to call while audio is playing. An empty source is silence.
  void setSource(Source source);
  /// For --stats: the seconds spent making sound since the last call (on whichever task SDL
  /// plays it from).
  double takeBusySeconds();

 private:
  static void SDLCALL callback(void* userdata, SDL_AudioStream* stream, int additionalAmount, int totalAmount);
  SDL_AudioStream* stream_ = nullptr;
  Mutex mutex_;
  Source source_;
  std::vector<float> scratch_;
  std::vector<std::int16_t> pcm_;      ///< (the Amiga: what AHI takes, made here rather than by SDL)
  std::atomic<std::uint64_t> busyNs_{0};
};

}  // namespace encore
