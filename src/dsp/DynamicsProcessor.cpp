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
  if (!std::isfinite(sample) || std::abs(sample) < DspConfig::denormalFloor) {
    return 0.0f;
  }
  return sample;
}

float finalSafetyClamp(float sample) {
  sample = sanitizeSample(sample);
  if (sample > 0.999f) {
    return 0.999f;
  }
  if (sample < -0.999f) {
    return -0.999f;
  }
  return sample;
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
  float blockPeak = 0.0f;
  uint64_t blockLimiterActivations = 0;
  uint64_t blockClippedSamples = 0;

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
      const float targetGain =
          outputPeak > currentCeiling && outputPeak > DspConfig::tinySignal
              ? currentCeiling / outputPeak
              : 1.0f;

      if (targetGain < limiterGain) {
        limiterGain = targetGain;
        ++blockLimiterActivations;
      } else {
        const float limiterRelease =
            timeCoefficient(DspConfig::limiterReleaseMs, sampleRate);
        limiterGain =
            limiterRelease * limiterGain + (1.0f - limiterRelease);
      }

      leftSample *= limiterGain;
      rightSample *= limiterGain;
    }

    leftSample = finalSafetyClamp(leftSample);
    rightSample = finalSafetyClamp(rightSample);
    blockPeak = std::max(
        blockPeak, std::max(std::abs(leftSample), std::abs(rightSample)));
    if (std::abs(leftSample) >= 0.999f) {
      ++blockClippedSamples;
    }
    if (std::abs(rightSample) >= 0.999f) {
      ++blockClippedSamples;
    }

    left[frame] = std::clamp(sanitizeSample(leftSample), DspConfig::outputMin,
                             DspConfig::outputMax);
    right[frame] = std::clamp(sanitizeSample(rightSample),
                              DspConfig::outputMin, DspConfig::outputMax);
  }

  recentPeak.store(blockPeak, std::memory_order_relaxed);
  if (blockLimiterActivations > 0) {
    limiterActivations.fetch_add(blockLimiterActivations,
                                 std::memory_order_relaxed);
  }
  if (blockClippedSamples > 0) {
    clippedSamples.fetch_add(blockClippedSamples, std::memory_order_relaxed);
  }
}

void DynamicsProcessor::reset() {
  envelope = 0.0f;
  limiterGain = 1.0f;
  resetMetering();
}

void DynamicsProcessor::resetMetering() {
  recentPeak.store(0.0f, std::memory_order_relaxed);
  limiterActivations.store(0, std::memory_order_relaxed);
  clippedSamples.store(0, std::memory_order_relaxed);
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

float DynamicsProcessor::recentPeakLinear() const {
  return recentPeak.load(std::memory_order_acquire);
}

uint64_t DynamicsProcessor::limiterActivationCount() const {
  return limiterActivations.load(std::memory_order_acquire);
}

uint64_t DynamicsProcessor::clippedSampleCount() const {
  return clippedSamples.load(std::memory_order_acquire);
}
