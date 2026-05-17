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

  bool compressorActive() const;
  bool limiterActive() const;
  float limiterCeilingLinear() const;

private:
  std::atomic<float> thresholdDb = DspConfig::defaultCompressorThresholdDb;
  std::atomic<float> ratio = DspConfig::defaultCompressorRatio;
  std::atomic<float> attackMs = DspConfig::defaultCompressorAttackMs;
  std::atomic<float> releaseMs = DspConfig::defaultCompressorReleaseMs;
  std::atomic<float> ceilingLinear = DspConfig::defaultLimiterCeilingLinear;
  std::atomic<bool> compressorEnabled = false;
  std::atomic<bool> limiterEnabled = true;

  float envelope = 0.0f;
};
