#include "AudioProcessor.hpp"

#include "../audio/AudioConfig.hpp"
#include "../core/Logger.hpp"
#include "DspConfig.hpp"

#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

float decibelsToLinear(float db) {
  return std::pow(10.0f, db / 20.0f);
}

float effectGainLinear(const Effect &effect) {
  const auto linear = effect.parameters.find("linear");
  if (linear != effect.parameters.end()) {
    return linear->second;
  }

  const auto db = effect.parameters.find("db");
  if (db != effect.parameters.end()) {
    return decibelsToLinear(db->second);
  }

  const auto gainDb = effect.parameters.find("gain_db");
  if (gainDb != effect.parameters.end()) {
    return decibelsToLinear(gainDb->second);
  }

  const auto gain = effect.parameters.find("gain");
  if (gain != effect.parameters.end()) {
    return decibelsToLinear(gain->second);
  }

  const auto preamp = effect.parameters.find("preamp");
  if (preamp != effect.parameters.end()) {
    return decibelsToLinear(preamp->second);
  }

  return 1.0f;
}

float parameterValue(const Effect &effect, const std::string &name,
                     float fallback) {
  const auto value = effect.parameters.find(name);
  return value == effect.parameters.end() ? fallback : value->second;
}

} // namespace

AudioProcessor::AudioProcessor() {
  ringBuffer.resize(AudioConfig::ringBufferFrames);
}

AudioProcessor::~AudioProcessor() {
  stop();
}

bool AudioProcessor::start(const std::string &sourceSinkName,
                           const std::string &targetSinkName) {
  std::lock_guard<std::mutex> lock(controlMutex);

  stopUnlocked();

  sourceSink = sourceSinkName;
  targetSink = targetSinkName;
  resetRingBuffer();

  loop = pw_thread_loop_new(AudioConfig::processorThreadLoopName.data(),
                            nullptr);
  if (!loop) {
    Logger::error("AudioProcessor failed to create PipeWire thread loop.");
    return false;
  }

  static const pw_stream_events captureEvents = {
      .version = PW_VERSION_STREAM_EVENTS,
      .state_changed = &AudioProcessor::onCaptureStateChanged,
      .param_changed = &AudioProcessor::onCaptureParamChanged,
      .process = &AudioProcessor::onCaptureProcess,
  };

  static const pw_stream_events playbackEvents = {
      .version = PW_VERSION_STREAM_EVENTS,
      .state_changed = &AudioProcessor::onPlaybackStateChanged,
      .param_changed = &AudioProcessor::onPlaybackParamChanged,
      .process = &AudioProcessor::onPlaybackProcess,
  };

  captureStream = pw_stream_new_simple(
      pw_thread_loop_get_loop(loop), AudioConfig::captureStreamName.data(),
      pw_properties_new(PW_KEY_MEDIA_TYPE, AudioConfig::mediaTypeAudio.data(),
                        PW_KEY_MEDIA_CATEGORY,
                        AudioConfig::captureCategory.data(),
                        PW_KEY_MEDIA_ROLE, AudioConfig::mediaRoleMusic.data(),
                        AudioConfig::streamCaptureSinkProperty.data(), "true",
                        AudioConfig::nodeAutoconnectProperty.data(), "true",
                        AudioConfig::nodeDontFallbackProperty.data(), "true",
                        AudioConfig::nodeDontMoveProperty.data(), "true",
                        PW_KEY_TARGET_OBJECT, sourceSink.c_str(), nullptr),
      &captureEvents, this);
  if (!captureStream) {
    Logger::error("AudioProcessor failed to create capture stream.");
    stopUnlocked();
    return false;
  }

  playbackStream = pw_stream_new_simple(
      pw_thread_loop_get_loop(loop), AudioConfig::playbackStreamName.data(),
      pw_properties_new(PW_KEY_MEDIA_TYPE, AudioConfig::mediaTypeAudio.data(),
                        PW_KEY_MEDIA_CATEGORY,
                        AudioConfig::playbackCategory.data(),
                        PW_KEY_MEDIA_ROLE, AudioConfig::mediaRoleMusic.data(),
                        AudioConfig::nodeAutoconnectProperty.data(), "true",
                        AudioConfig::nodeDontFallbackProperty.data(), "true",
                        AudioConfig::nodeDontMoveProperty.data(), "true",
                        PW_KEY_TARGET_OBJECT, targetSink.c_str(), nullptr),
      &playbackEvents, this);
  if (!playbackStream) {
    Logger::error("AudioProcessor failed to create playback stream.");
    stopUnlocked();
    return false;
  }

  std::array<uint8_t, 1024> captureBuffer{};
  spa_pod_builder captureBuilder =
      SPA_POD_BUILDER_INIT(captureBuffer.data(), captureBuffer.size());
  spa_audio_info_raw captureInfo{};
  captureInfo.format = SPA_AUDIO_FORMAT_F32;
  captureInfo.rate = AudioConfig::defaultSampleRate;
  captureInfo.channels = AudioConfig::channelCount;
  captureInfo.position[0] = SPA_AUDIO_CHANNEL_FL;
  captureInfo.position[1] = SPA_AUDIO_CHANNEL_FR;
  const spa_pod *captureParams[1] = {
      spa_format_audio_raw_build(&captureBuilder, SPA_PARAM_EnumFormat,
                                 &captureInfo)};

  std::array<uint8_t, 1024> playbackBuffer{};
  spa_pod_builder playbackBuilder =
      SPA_POD_BUILDER_INIT(playbackBuffer.data(), playbackBuffer.size());
  spa_audio_info_raw playbackInfo = captureInfo;
  const spa_pod *playbackParams[1] = {
      spa_format_audio_raw_build(&playbackBuilder, SPA_PARAM_EnumFormat,
                                 &playbackInfo)};

  if (pw_stream_connect(captureStream, PW_DIRECTION_INPUT, PW_ID_ANY,
                        static_cast<pw_stream_flags>(
                            PW_STREAM_FLAG_AUTOCONNECT |
                            PW_STREAM_FLAG_MAP_BUFFERS |
                            PW_STREAM_FLAG_RT_PROCESS),
                        captureParams, 1) < 0) {
    Logger::error("AudioProcessor failed to connect capture stream.");
    stopUnlocked();
    return false;
  }

  if (pw_stream_connect(playbackStream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                        static_cast<pw_stream_flags>(
                            PW_STREAM_FLAG_AUTOCONNECT |
                            PW_STREAM_FLAG_MAP_BUFFERS |
                            PW_STREAM_FLAG_RT_PROCESS),
                        playbackParams, 1) < 0) {
    Logger::error("AudioProcessor failed to connect playback stream.");
    stopUnlocked();
    return false;
  }

  if (pw_thread_loop_start(loop) < 0) {
    Logger::error("AudioProcessor failed to start PipeWire thread loop.");
    stopUnlocked();
    return false;
  }
  loopStarted = true;

  running.store(true, std::memory_order_release);
  Logger::info("AudioProcessor started: " + sourceSink + " -> " +
               targetSink);
  Logger::info("AudioProcessor requested F32 stereo at " +
               std::to_string(AudioConfig::defaultSampleRate) + " Hz.");
  return true;
}

void AudioProcessor::stop() {
  std::lock_guard<std::mutex> lock(controlMutex);
  stopUnlocked();
}

void AudioProcessor::stopUnlocked() {
  if (loop && loopStarted) {
    pw_thread_loop_stop(loop);
    loopStarted = false;
  }

  if (captureStream) {
    pw_stream_destroy(captureStream);
    captureStream = nullptr;
  }

  if (playbackStream) {
    pw_stream_destroy(playbackStream);
    playbackStream = nullptr;
  }

  if (loop) {
    pw_thread_loop_destroy(loop);
    loop = nullptr;
  }

  running.store(false, std::memory_order_release);
  captureStreaming.store(false, std::memory_order_release);
  playbackStreaming.store(false, std::memory_order_release);
  formatSupported.store(false, std::memory_order_release);
  captureFormatSupported.store(false, std::memory_order_release);
  playbackFormatSupported.store(false, std::memory_order_release);
  sourceSink.clear();
  targetSink.clear();
  resetRingBuffer();
}

void AudioProcessor::setEffectChain(const EffectChain &chain) {
  float gain = 1.0f;
  std::vector<EQBand> bands;
  bands.reserve(ParametricEQ::maxBands);
  bool compressorConfigured = false;
  bool limiterConfigured = false;
  float compressorThresholdDb = DspConfig::defaultCompressorThresholdDb;
  float compressorRatio = DspConfig::defaultCompressorRatio;
  float compressorAttackMs = DspConfig::defaultCompressorAttackMs;
  float compressorReleaseMs = DspConfig::defaultCompressorReleaseMs;
  float limiterCeilingDb = DspConfig::defaultLimiterCeilingDb;
  float width = 1.0f;

  for (const auto &effect : chain.effects) {
    if (effect.type == "gain" || effect.type == "preamp") {
      gain *= effectGainLinear(effect);
    } else if (effect.type == "eq_band") {
      EQBand band;
      const auto freq = effect.parameters.find("freq");
      const auto gainDb = effect.parameters.find("gain");
      const auto q = effect.parameters.find("q");

      if (freq != effect.parameters.end()) {
        band.frequency = freq->second;
      } else if (bands.size() < DspConfig::eqFrequencies.size()) {
        band.frequency = DspConfig::eqFrequencies[bands.size()];
      }

      if (gainDb != effect.parameters.end()) {
        band.gainDb = gainDb->second;
      }
      if (q != effect.parameters.end()) {
        band.q = q->second;
      }

      band.enabled = true;
      bands.push_back(band);
    } else if (effect.type == "compressor") {
      compressorConfigured = true;
      compressorThresholdDb =
          parameterValue(effect, "threshold", compressorThresholdDb);
      compressorThresholdDb =
          parameterValue(effect, "threshold_db", compressorThresholdDb);
      compressorRatio = parameterValue(effect, "ratio", compressorRatio);
      compressorAttackMs = parameterValue(effect, "attack", compressorAttackMs);
      compressorAttackMs =
          parameterValue(effect, "attack_ms", compressorAttackMs);
      compressorReleaseMs =
          parameterValue(effect, "release", compressorReleaseMs);
      compressorReleaseMs =
          parameterValue(effect, "release_ms", compressorReleaseMs);
    } else if (effect.type == "limiter") {
      limiterConfigured = true;
      limiterCeilingDb = parameterValue(effect, "ceiling", limiterCeilingDb);
      limiterCeilingDb =
          parameterValue(effect, "ceiling_db", limiterCeilingDb);
    } else if (effect.type == "stereo_width" || effect.type == "spatial") {
      width = parameterValue(effect, "width", width);
    }
  }

  gain = std::clamp(gain, DspConfig::minProcessorGainLinear,
                    DspConfig::maxProcessorGainLinear);
  width = std::clamp(width, DspConfig::minStereoWidth,
                     DspConfig::maxStereoWidth);
  gainLinear.store(gain, std::memory_order_release);
  stereoWidth.store(width, std::memory_order_release);
  const uint32_t currentSampleRate =
      sampleRate.load(std::memory_order_acquire);
  equalizer.setBands(bands, static_cast<float>(currentSampleRate));
  dynamics.configureCompressor(compressorThresholdDb, compressorRatio,
                               compressorAttackMs, compressorReleaseMs,
                               compressorConfigured);
  dynamics.configureLimiter(limiterCeilingDb, true);
  Logger::info("AudioProcessor gain set to linear value: " +
               std::to_string(gain));
  Logger::info("AudioProcessor stereo width set to: " +
               std::to_string(width));
  Logger::info("AudioProcessor EQ updated. Sample rate: " +
               std::to_string(currentSampleRate) + " Hz, active bands: " +
               std::to_string(equalizer.activeBandCount()) +
               ", auto preamp: " + std::to_string(equalizer.preampLinear()));
  Logger::info("AudioProcessor dynamics updated. Compressor: " +
               std::string(compressorConfigured ? "enabled" : "disabled") +
               " threshold " + std::to_string(compressorThresholdDb) +
               " dB, ratio " + std::to_string(compressorRatio) +
               ":1, attack " + std::to_string(compressorAttackMs) +
               " ms, release " + std::to_string(compressorReleaseMs) +
               " ms. Limiter: " +
               std::string(limiterConfigured ? "preset" : "default") +
               " ceiling " + std::to_string(limiterCeilingDb) + " dB.");
}

bool AudioProcessor::isRunning() const {
  return running.load(std::memory_order_acquire);
}

void AudioProcessor::onCaptureProcess(void *data) {
  static_cast<AudioProcessor *>(data)->processCapture();
}

void AudioProcessor::onPlaybackProcess(void *data) {
  static_cast<AudioProcessor *>(data)->processPlayback();
}

void AudioProcessor::onCaptureStateChanged(void *data, pw_stream_state,
                                           pw_stream_state state,
                                           const char *) {
  auto *processor = static_cast<AudioProcessor *>(data);
  processor->captureStreaming.store(state == PW_STREAM_STATE_STREAMING,
                                    std::memory_order_release);
  if (state == PW_STREAM_STATE_ERROR) {
    processor->running.store(false, std::memory_order_release);
  }
}

void AudioProcessor::onPlaybackStateChanged(void *data, pw_stream_state,
                                            pw_stream_state state,
                                            const char *) {
  auto *processor = static_cast<AudioProcessor *>(data);
  processor->playbackStreaming.store(state == PW_STREAM_STATE_STREAMING,
                                     std::memory_order_release);
  if (state == PW_STREAM_STATE_ERROR) {
    processor->running.store(false, std::memory_order_release);
  }
}

void AudioProcessor::onCaptureParamChanged(void *data, uint32_t id,
                                           const spa_pod *param) {
  if (id == SPA_PARAM_Format) {
    static_cast<AudioProcessor *>(data)->updateNegotiatedFormat(param, true);
  }
}

void AudioProcessor::onPlaybackParamChanged(void *data, uint32_t id,
                                            const spa_pod *param) {
  if (id == SPA_PARAM_Format) {
    static_cast<AudioProcessor *>(data)->updateNegotiatedFormat(param, false);
  }
}

void AudioProcessor::processCapture() {
  pw_buffer *pipewireBuffer = pw_stream_dequeue_buffer(captureStream);
  if (!pipewireBuffer) {
    return;
  }

  if (!formatSupported.load(std::memory_order_acquire)) {
    pw_stream_queue_buffer(captureStream, pipewireBuffer);
    return;
  }

  spa_buffer *buffer = pipewireBuffer->buffer;
  if (!buffer || buffer->n_datas == 0 || !buffer->datas[0].data ||
      !buffer->datas[0].chunk) {
    pw_stream_queue_buffer(captureStream, pipewireBuffer);
    return;
  }

  const spa_chunk *chunk = buffer->datas[0].chunk;
  const auto *bytes =
      static_cast<const uint8_t *>(buffer->datas[0].data) + chunk->offset;
  const auto *samples = reinterpret_cast<const float *>(bytes);
  const std::size_t sampleCount = chunk->size / sizeof(float);

  writeProcessedSamples(samples, sampleCount);
  pw_stream_queue_buffer(captureStream, pipewireBuffer);
}

void AudioProcessor::processPlayback() {
  pw_buffer *pipewireBuffer = pw_stream_dequeue_buffer(playbackStream);
  if (!pipewireBuffer) {
    return;
  }

  spa_buffer *buffer = pipewireBuffer->buffer;
  if (!buffer || buffer->n_datas == 0 || !buffer->datas[0].data ||
      !buffer->datas[0].chunk) {
    pw_stream_queue_buffer(playbackStream, pipewireBuffer);
    return;
  }

  const bool canProcess = formatSupported.load(std::memory_order_acquire);
  const uint32_t requestedFrames =
      pipewireBuffer->requested == 0 ? 256 : pipewireBuffer->requested;
  const std::size_t maxFrames =
      (buffer->datas[0].maxsize / sizeof(float)) / AudioConfig::channelCount;
  const std::size_t frames = std::min<std::size_t>(requestedFrames, maxFrames);
  const std::size_t sampleCount = frames * AudioConfig::channelCount;

  auto *samples = static_cast<float *>(buffer->datas[0].data);
  if (canProcess) {
    readSamples(samples, sampleCount);
  } else {
    std::fill(samples, samples + sampleCount, 0.0f);
  }

  spa_chunk *chunk = buffer->datas[0].chunk;
  chunk->offset = 0;
  chunk->stride =
      static_cast<int32_t>(sizeof(float) * AudioConfig::channelCount);
  chunk->size = static_cast<uint32_t>(sampleCount * sizeof(float));

  pw_stream_queue_buffer(playbackStream, pipewireBuffer);
}

void AudioProcessor::writeProcessedSamples(const float *samples,
                                           std::size_t sampleCount) {
  const float gain = gainLinear.load(std::memory_order_relaxed);
  const float width = stereoWidth.load(std::memory_order_relaxed);
  const std::size_t totalFrames = sampleCount / AudioConfig::channelCount;
  std::size_t processedFrames = 0;
  const float currentSampleRate =
      static_cast<float>(sampleRate.load(std::memory_order_relaxed));

  while (processedFrames < totalFrames) {
    const std::size_t frames =
        std::min(totalFrames - processedFrames, leftScratch.size());

    for (std::size_t frame = 0; frame < frames; ++frame) {
      const std::size_t index =
          (processedFrames + frame) * AudioConfig::channelCount;
      leftScratch[frame] = samples[index] * gain;
      rightScratch[frame] = samples[index + 1] * gain;
    }

    equalizer.process(leftScratch.data(), rightScratch.data(),
                      static_cast<uint32_t>(frames));

    if (std::abs(width - 1.0f) > 0.001f) {
      for (std::size_t frame = 0; frame < frames; ++frame) {
        const float left = leftScratch[frame];
        const float right = rightScratch[frame];
        const float mid = (left + right) * 0.5f;
        const float side = (left - right) * 0.5f * width;
        leftScratch[frame] = mid + side;
        rightScratch[frame] = mid - side;
      }
    }

    dynamics.process(leftScratch.data(), rightScratch.data(),
                     static_cast<uint32_t>(frames), currentSampleRate);

    for (std::size_t frame = 0; frame < frames; ++frame) {
      ringBuffer.push(
          {std::clamp(std::isfinite(leftScratch[frame]) ? leftScratch[frame]
                                                        : 0.0f,
                      DspConfig::outputMin, DspConfig::outputMax),
           std::clamp(std::isfinite(rightScratch[frame]) ? rightScratch[frame]
                                                         : 0.0f,
                      DspConfig::outputMin, DspConfig::outputMax)});
    }

    processedFrames += frames;
  }
}

void AudioProcessor::readSamples(float *samples, std::size_t sampleCount) {
  const std::size_t frames = sampleCount / AudioConfig::channelCount;
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const StereoFrame stereoFrame = ringBuffer.pop();
    const std::size_t index = frame * AudioConfig::channelCount;
    samples[index] = stereoFrame.left;
    samples[index + 1] = stereoFrame.right;
  }
}

void AudioProcessor::resetRingBuffer() {
  ringBuffer.clear();
  equalizer.reset();
  dynamics.reset();
}

void AudioProcessor::updateNegotiatedFormat(const spa_pod *param,
                                            bool isCapture) {
  spa_audio_info_raw info{};
  if (!param || spa_format_audio_raw_parse(param, &info) < 0) {
    if (isCapture) {
      captureFormatSupported.store(false, std::memory_order_release);
    } else {
      playbackFormatSupported.store(false, std::memory_order_release);
    }
    formatSupported.store(false, std::memory_order_release);
    running.store(false, std::memory_order_release);
    return;
  }

  const bool streamSupported = info.format == SPA_AUDIO_FORMAT_F32 &&
                               info.channels == AudioConfig::channelCount &&
                               info.rate > 0;
  if (isCapture) {
    captureFormatSupported.store(streamSupported, std::memory_order_release);
    if (streamSupported) {
      captureSampleRate.store(info.rate, std::memory_order_release);
    }
  } else {
    playbackFormatSupported.store(streamSupported, std::memory_order_release);
    if (streamSupported) {
      playbackSampleRate.store(info.rate, std::memory_order_release);
    }
  }

  if (!streamSupported) {
    formatSupported.store(false, std::memory_order_release);
    running.store(false, std::memory_order_release);
    return;
  }

  const uint32_t captureRate =
      captureSampleRate.load(std::memory_order_acquire);
  const uint32_t playbackRate =
      playbackSampleRate.load(std::memory_order_acquire);
  const bool supported =
      captureFormatSupported.load(std::memory_order_acquire) &&
      playbackFormatSupported.load(std::memory_order_acquire) &&
      captureRate == playbackRate;
  if (supported) {
    sampleRate.store(captureRate, std::memory_order_release);
    channelCount.store(AudioConfig::channelCount, std::memory_order_release);
  }

  formatSupported.store(supported, std::memory_order_release);
  if (captureFormatSupported.load(std::memory_order_acquire) &&
      playbackFormatSupported.load(std::memory_order_acquire) && !supported) {
    running.store(false, std::memory_order_release);
  }
}
