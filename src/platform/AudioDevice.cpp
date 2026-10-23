#include "platform/AudioDevice.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <utility>

#include "core/Log.h"

namespace encore {

AudioDevice::~AudioDevice() {
  log::debug("leaving: the sound device");
  close();
}

bool AudioDevice::open(int sampleRate) {
  SDL_AudioSpec spec{};
#ifdef __amigaos__
  // 16 bits in the machine's order, as AHI plays them: SDL passes them on as they are, rather
  // than converting the floats itself.
  spec.format = SDL_AUDIO_S16;
#else
  spec.format = SDL_AUDIO_F32;
#endif
  // (made once, here: the sound is made on SDL's task, which must not allocate -- libnix's
  // malloc is the main task's)
  scratch_.assign(kChunk * 2, 0.0f);
  pcm_.assign(kChunk * 2, 0);
  spec.channels = 2;
  spec.freq = sampleRate;
  // The card is asked to take 512 samples at a time, about 11 ms: every sound the game makes is
  // heard one helping after it is asked for, so less would be heard sooner, but Linux's sound
  // servers crackle at half of it. An environment variable of the same name still wins.
#ifdef __amigaos__
  // On the Amiga a helping is about 43 ms whatever the rate (1024 samples at 24000, 2048 at
  // 48000): less, and the audio task, at priority 10, wakes too often for the main one and AHI
  // runs dry; more, and the effects bunch at the start of each helping (Pinball 2026-10-21).
  {
    int frames = 256;
    while (frames * 2 * 10000 <= sampleRate * 43 * 14) frames *= 2;  // (the power of two nearest 43 ms)
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, std::to_string(frames).c_str());
  }
#else
  SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");
#endif
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &AudioDevice::callback, this);
  if (!stream_) {
    log::error(std::string("SDL_OpenAudioDeviceStream: ") + SDL_GetError());
    return false;
  }
  SDL_ResumeAudioStreamDevice(stream_);
  return true;
}

void AudioDevice::close() {
  if (stream_) {
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
  }
}

void AudioDevice::setSource(Source source) {
  std::lock_guard lock(mutex_);
  source_ = std::move(source);
}

void SDLCALL AudioDevice::callback(void* userdata, SDL_AudioStream* stream, int additionalAmount, int) {
  auto* self = static_cast<AudioDevice*>(userdata);
  const auto began = std::chrono::steady_clock::now();
#ifdef __amigaos__
  constexpr int kFrameBytes = static_cast<int>(sizeof(std::int16_t) * 2);
#else
  constexpr int kFrameBytes = static_cast<int>(sizeof(float) * 2);
#endif
  for (int left = additionalAmount / kFrameBytes; left > 0;) {
    const int frames = std::min(left, static_cast<int>(kChunk));
    {
      std::lock_guard lock(self->mutex_);
      if (self->source_)
        self->source_(self->scratch_.data(), frames);
      else
        std::fill_n(self->scratch_.begin(), frames * 2, 0.0f);
    }
#ifdef __amigaos__
    // (the sound is within -1 to 1 already: MusicDriver::renderAt)
    const float* in = self->scratch_.data();
    std::int16_t* out = self->pcm_.data();
    for (int i = 0; i < frames * 2; ++i) out[i] = static_cast<std::int16_t>(in[i] * 32767.0f);
    SDL_PutAudioStreamData(stream, out, frames * kFrameBytes);
#else
    SDL_PutAudioStreamData(stream, self->scratch_.data(), frames * kFrameBytes);
#endif
    left -= frames;
  }
  self->busyNs_.fetch_add(static_cast<std::uint64_t>(
                              std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - began).count()),
                          std::memory_order_relaxed);
}

double AudioDevice::takeBusySeconds() { return static_cast<double>(busyNs_.exchange(0, std::memory_order_relaxed)) / 1e9; }

}  // namespace encore
