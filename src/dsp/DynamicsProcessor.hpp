#pragma once

#include "DspConfig.hpp"

#include <atomic>
#include <cstdint>

class DynamicsProcessor {
public:
  void configureCompressor(float thresholdDb, float ratio, float attackMs,
                           float releaseMs, bool enabled);
  void configureLimiter(float ceilingDb, bool enabled);
  void process(float *left, float *right, uint32_t frames, float sampleRate);
  void reset();
  void resetMetering();

  bool compressorActive() const;
  bool limiterActive() const;
  float limiterCeilingLinear() const;
  float recentPeakLinear() const;
  uint64_t limiterActivationCount() const;
  uint64_t clippedSampleCount() const;

private:
  std::atomic<float> thresholdDb = DspConfig::defaultCompressorThresholdDb;
  std::atomic<float> ratio = DspConfig::defaultCompressorRatio;
  std::atomic<float> attackMs = DspConfig::defaultCompressorAttackMs;
  std::atomic<float> releaseMs = DspConfig::defaultCompressorReleaseMs;
  std::atomic<float> ceilingLinear = DspConfig::defaultLimiterCeilingLinear;
  std::atomic<bool> compressorEnabled = false;
  std::atomic<bool> limiterEnabled = true;
  std::atomic<float> recentPeak = 0.0f;
  std::atomic<uint64_t> limiterActivations = 0;
  std::atomic<uint64_t> clippedSamples = 0;

  float envelope = 0.0f;
  float limiterGain = 1.0f;
};
