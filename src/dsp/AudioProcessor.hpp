#pragma once

#include "audio/AudioConfig.hpp"
#include "audio/EffectChain.hpp"
#include "dsp/DspConfig.hpp"
#include "dsp/DynamicsProcessor.hpp"
#include "dsp/ParametricEQ.hpp"
#include "dsp/SpscFrameRingBuffer.hpp"

#include <pipewire/stream.h>

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

struct pw_buffer;
struct pw_stream;
struct pw_thread_loop;
struct spa_pod;

class AudioProcessor {
public:
  AudioProcessor();
  ~AudioProcessor();

  bool start(const std::string &sourceSink, const std::string &targetSink);
  void stop();
  void setEffectChain(const EffectChain &chain);
  bool isRunning() const;

private:
  static void onCaptureProcess(void *data);
  static void onPlaybackProcess(void *data);
  static void onCaptureStateChanged(void *data, pw_stream_state oldState,
                                    pw_stream_state state, const char *error);
  static void onPlaybackStateChanged(void *data, pw_stream_state oldState,
                                     pw_stream_state state, const char *error);
  static void onCaptureParamChanged(void *data, uint32_t id,
                                    const struct spa_pod *param);
  static void onPlaybackParamChanged(void *data, uint32_t id,
                                     const struct spa_pod *param);

  void processCapture();
  void processPlayback();
  void writeProcessedSamples(const float *samples, std::size_t sampleCount);
  void readSamples(float *samples, std::size_t sampleCount);
  void resetRingBuffer();
  void updateNegotiatedFormat(const struct spa_pod *param, bool isCapture);
  void stopUnlocked();

private:
  mutable std::mutex controlMutex;

  pw_thread_loop *loop = nullptr;
  pw_stream *captureStream = nullptr;
  pw_stream *playbackStream = nullptr;
  bool loopStarted = false;

  std::atomic_bool running = false;
  std::atomic_bool captureStreaming = false;
  std::atomic_bool playbackStreaming = false;
  std::atomic_bool formatSupported = false;
  std::atomic_bool captureFormatSupported = false;
  std::atomic_bool playbackFormatSupported = false;
  std::atomic<uint32_t> captureSampleRate = AudioConfig::defaultSampleRate;
  std::atomic<uint32_t> playbackSampleRate = AudioConfig::defaultSampleRate;
  std::atomic<uint32_t> sampleRate = AudioConfig::defaultSampleRate;
  std::atomic<uint32_t> channelCount = AudioConfig::channelCount;
  std::atomic<float> gainLinear = 1.0f;
  std::atomic<float> stereoWidth = 1.0f;

  std::string sourceSink;
  std::string targetSink;

  SpscFrameRingBuffer ringBuffer;
  std::array<float, AudioConfig::scratchFrames> leftScratch{};
  std::array<float, AudioConfig::scratchFrames> rightScratch{};
  ParametricEQ equalizer;
  DynamicsProcessor dynamics;
};
