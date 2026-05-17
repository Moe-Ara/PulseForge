#include "DynamicsProcessor.hpp"

#include "DspConfig.hpp"

#include <algorithm>
#include <cmath>

namespace {

float dbToLinear(float db) {
  return std::pow(10.0f, db / 20.0f);
}

float linearToDb(float value) {
  return 20.0f * std::log10(std::max(value, DspConfig::tinySignal));
}

float timeCoefficient(float milliseconds, float sampleRate) {
  milliseconds = std::max(milliseconds, 0.1f);
  sampleRate = std::max(sampleRate, 1.0f);
  return std::exp(-1.0f / ((milliseconds / 1000.0f) * sampleRate));
}

float sanitizeSample(float sample) {
  return std::isfinite(sample) ? sample : 0.0f;
}

} // namespace

void DynamicsProcessor::configureCompressor(float thresholdDbValue,
                                            float ratioValue,
                                            float attackMsValue,
                                            float releaseMsValue,
                                            bool enabled) {
  thresholdDb.store(std::clamp(thresholdDbValue,
                               DspConfig::minCompressorThresholdDb,
                               DspConfig::maxCompressorThresholdDb),
                    std::memory_order_release);
  ratio.store(std::clamp(ratioValue, DspConfig::minCompressorRatio,
                         DspConfig::maxCompressorRatio),
              std::memory_order_release);
  attackMs.store(std::clamp(attackMsValue, DspConfig::minCompressorAttackMs,
                            DspConfig::maxCompressorAttackMs),
                 std::memory_order_release);
  releaseMs.store(std::clamp(releaseMsValue, DspConfig::minCompressorReleaseMs,
                             DspConfig::maxCompressorReleaseMs),
                  std::memory_order_release);
  compressorEnabled.store(enabled, std::memory_order_release);
}

void DynamicsProcessor::configureLimiter(float ceilingDb, bool enabled) {
  ceilingDb = std::clamp(ceilingDb, DspConfig::minLimiterCeilingDb,
                         DspConfig::maxLimiterCeilingDb);
  ceilingLinear.store(dbToLinear(ceilingDb), std::memory_order_release);
  limiterEnabled.store(enabled, std::memory_order_release);
}

void DynamicsProcessor::process(float *left, float *right, uint32_t frames,
                                float sampleRate) {
  const bool useCompressor =
      compressorEnabled.load(std::memory_order_acquire);
  const bool useLimiter = limiterEnabled.load(std::memory_order_acquire);
  const float currentThresholdDb =
      thresholdDb.load(std::memory_order_relaxed);
  const float currentRatio = ratio.load(std::memory_order_relaxed);
  const float currentCeiling =
      ceilingLinear.load(std::memory_order_relaxed);
  const float currentThresholdLinear = dbToLinear(currentThresholdDb);

  const float attack =
      timeCoefficient(attackMs.load(std::memory_order_relaxed), sampleRate);
  const float release =
      timeCoefficient(releaseMs.load(std::memory_order_relaxed), sampleRate);

  for (uint32_t frame = 0; frame < frames; ++frame) {
    float leftSample = sanitizeSample(left[frame]);
    float rightSample = sanitizeSample(right[frame]);
    const float inputPeak =
        std::max(std::abs(leftSample), std::abs(rightSample));

    const float coefficient = inputPeak > envelope ? attack : release;
    envelope = coefficient * envelope + (1.0f - coefficient) * inputPeak;

    if (useCompressor && currentRatio > 1.0f &&
        envelope > currentThresholdLinear) {
      const float envelopeDb = linearToDb(envelope);
      if (envelopeDb > currentThresholdDb) {
        const float compressedDb =
            currentThresholdDb + (envelopeDb - currentThresholdDb) /
                                     currentRatio;
        const float gain = dbToLinear(compressedDb - envelopeDb);
        leftSample *= gain;
        rightSample *= gain;
      }
    }

    if (useLimiter) {
      const float outputPeak =
          std::max(std::abs(leftSample), std::abs(rightSample));
      if (outputPeak > currentCeiling && outputPeak > DspConfig::tinySignal) {
        const float gain = currentCeiling / outputPeak;
        leftSample *= gain;
        rightSample *= gain;
      }
    }

    left[frame] = std::clamp(sanitizeSample(leftSample), DspConfig::outputMin,
                             DspConfig::outputMax);
    right[frame] = std::clamp(sanitizeSample(rightSample),
                              DspConfig::outputMin, DspConfig::outputMax);
  }
}

void DynamicsProcessor::reset() {
  envelope = 0.0f;
}

bool DynamicsProcessor::compressorActive() const {
  return compressorEnabled.load(std::memory_order_acquire);
}

bool DynamicsProcessor::limiterActive() const {
  return limiterEnabled.load(std::memory_order_acquire);
}

float DynamicsProcessor::limiterCeilingLinear() const {
  return ceilingLinear.load(std::memory_order_acquire);
}
