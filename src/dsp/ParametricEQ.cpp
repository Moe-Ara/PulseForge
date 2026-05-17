#include "ParametricEQ.hpp"

#include "DspConfig.hpp"

#include <algorithm>
#include <cmath>

namespace {

struct Coefficients {
  float b0 = 1.0f;
  float b1 = 0.0f;
  float b2 = 0.0f;
  float a1 = 0.0f;
  float a2 = 0.0f;
};

float dbToLinear(float db) {
  return std::pow(10.0f, db / 20.0f);
}

Coefficients peakingCoefficients(EQBand band, float sampleRate) {
  sampleRate = std::max(sampleRate, 1.0f);
  band.frequency =
      std::clamp(band.frequency, DspConfig::minFilterFrequencyHz,
                 sampleRate * DspConfig::maxFilterNyquistRatio);
  band.q = std::clamp(band.q, DspConfig::minEqQ, DspConfig::maxEqQ);
  band.gainDb = std::clamp(band.gainDb, DspConfig::minEqGainDb,
                           DspConfig::maxEqGainDb);

  const float a = std::pow(10.0f, band.gainDb / 40.0f);
  const float omega = 2.0f * DspConfig::pi * band.frequency / sampleRate;
  const float sinOmega = std::sin(omega);
  const float cosOmega = std::cos(omega);
  const float alpha = sinOmega / (2.0f * band.q);

  const float rawB0 = 1.0f + alpha * a;
  const float rawB1 = -2.0f * cosOmega;
  const float rawB2 = 1.0f - alpha * a;
  const float rawA0 = 1.0f + alpha / a;
  const float rawA1 = -2.0f * cosOmega;
  const float rawA2 = 1.0f - alpha / a;

  if (!std::isfinite(rawA0) || std::abs(rawA0) < 0.000001f) {
    return {};
  }

  return {rawB0 / rawA0, rawB1 / rawA0, rawB2 / rawA0, rawA1 / rawA0,
          rawA2 / rawA0};
}

} // namespace

void ParametricEQ::setBands(const std::vector<EQBand> &bands,
                            float sampleRate) {
  const std::size_t count = std::min(bands.size(), maxBands);
  float totalBoostDb = 0.0f;
  float peakBoostDb = 0.0f;
  std::size_t enabledCount = 0;
  const uint32_t currentIndex =
      activeConfigIndex.load(std::memory_order_acquire);
  const uint32_t nextIndex = currentIndex == 0 ? 1 : 0;
  EqConfig &config = configs[nextIndex];
  const uint32_t nextSequence =
      config.sequence.load(std::memory_order_relaxed) + 1;

  config.sequence.store(nextSequence | 1U, std::memory_order_release);

  for (std::size_t i = 0; i < maxBands; ++i) {
    const bool enabled = i < count && bands[i].enabled &&
                         std::abs(bands[i].gainDb) >
                             DspConfig::insignificantGainDb;

    if (!enabled) {
      config.bands[i].b0.store(1.0f, std::memory_order_relaxed);
      config.bands[i].b1.store(0.0f, std::memory_order_relaxed);
      config.bands[i].b2.store(0.0f, std::memory_order_relaxed);
      config.bands[i].a1.store(0.0f, std::memory_order_relaxed);
      config.bands[i].a2.store(0.0f, std::memory_order_relaxed);
      config.bands[i].enabled.store(false, std::memory_order_relaxed);
      continue;
    }

    EQBand safeBand = bands[i];
    safeBand.gainDb = std::clamp(safeBand.gainDb, DspConfig::minEqGainDb,
                                 DspConfig::maxEqGainDb);
    const Coefficients coefficients = peakingCoefficients(safeBand, sampleRate);

    config.bands[i].b0.store(coefficients.b0, std::memory_order_relaxed);
    config.bands[i].b1.store(coefficients.b1, std::memory_order_relaxed);
    config.bands[i].b2.store(coefficients.b2, std::memory_order_relaxed);
    config.bands[i].a1.store(coefficients.a1, std::memory_order_relaxed);
    config.bands[i].a2.store(coefficients.a2, std::memory_order_relaxed);
    config.bands[i].enabled.store(true, std::memory_order_relaxed);

    const float positiveBoostDb = std::max(0.0f, safeBand.gainDb);
    totalBoostDb += positiveBoostDb;
    peakBoostDb = std::max(peakBoostDb, positiveBoostDb);
    ++enabledCount;
  }

  const float chargeableBoostDb =
      std::max(0.0f, totalBoostDb - DspConfig::autoHeadroomFreeBoostDb);
  const float autoHeadroomDb =
      -std::min(DspConfig::maxAutoHeadroomDb,
                chargeableBoostDb * DspConfig::autoHeadroomTotalBoostRatio +
                    peakBoostDb * DspConfig::autoHeadroomPeakBoostRatio);
  const float preamp = dbToLinear(autoHeadroomDb);
  config.preamp.store(preamp, std::memory_order_relaxed);
  config.activeBands.store(enabledCount, std::memory_order_relaxed);
  config.sequence.store((nextSequence + 1U) & ~1U, std::memory_order_release);
  activeConfigIndex.store(nextIndex, std::memory_order_release);
  outputPreamp.store(preamp, std::memory_order_release);
  activeBands.store(enabledCount, std::memory_order_release);
}

void ParametricEQ::process(float *left, float *right, uint32_t frames) {
  refreshCoefficientsIfNeeded();

  const float preamp = outputPreamp.load(std::memory_order_relaxed);
  for (uint32_t frame = 0; frame < frames; ++frame) {
    float leftSample = std::isfinite(left[frame]) ? left[frame] * preamp : 0.0f;
    float rightSample =
        std::isfinite(right[frame]) ? right[frame] * preamp : 0.0f;

    for (std::size_t band = 0; band < maxBands; ++band) {
      if (!localEnabled[band]) {
        continue;
      }
      leftSample = leftFilters[band].process(leftSample);
      rightSample = rightFilters[band].process(rightSample);
    }

    left[frame] = std::isfinite(leftSample) ? leftSample : 0.0f;
    right[frame] = std::isfinite(rightSample) ? rightSample : 0.0f;
  }
}

void ParametricEQ::reset() {
  for (auto &filter : leftFilters) {
    filter.reset();
  }
  for (auto &filter : rightFilters) {
    filter.reset();
  }
}

float ParametricEQ::preampLinear() const {
  return outputPreamp.load(std::memory_order_acquire);
}

std::size_t ParametricEQ::activeBandCount() const {
  return activeBands.load(std::memory_order_acquire);
}

void ParametricEQ::refreshCoefficientsIfNeeded() {
  const uint32_t index = activeConfigIndex.load(std::memory_order_acquire);
  EqConfig &config = configs[index];
  const uint32_t sequence = config.sequence.load(std::memory_order_acquire);
  if ((sequence & 1U) != 0 || (index == localConfigIndex &&
                               sequence == localSequence)) {
    return;
  }

  std::array<bool, maxBands> nextEnabled{};
  std::array<float, maxBands> b0{};
  std::array<float, maxBands> b1{};
  std::array<float, maxBands> b2{};
  std::array<float, maxBands> a1{};
  std::array<float, maxBands> a2{};

  for (std::size_t band = 0; band < maxBands; ++band) {
    nextEnabled[band] =
        config.bands[band].enabled.load(std::memory_order_relaxed);
    b0[band] = config.bands[band].b0.load(std::memory_order_relaxed);
    b1[band] = config.bands[band].b1.load(std::memory_order_relaxed);
    b2[band] = config.bands[band].b2.load(std::memory_order_relaxed);
    a1[band] = config.bands[band].a1.load(std::memory_order_relaxed);
    a2[band] = config.bands[band].a2.load(std::memory_order_relaxed);
  }

  if (config.sequence.load(std::memory_order_acquire) != sequence) {
    return;
  }

  outputPreamp.store(config.preamp.load(std::memory_order_relaxed),
                     std::memory_order_relaxed);
  activeBands.store(config.activeBands.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);

  for (std::size_t band = 0; band < maxBands; ++band) {
    localEnabled[band] = nextEnabled[band];
    leftFilters[band].setCoefficients(b0[band], b1[band], b2[band], a1[band],
                                      a2[band]);
    rightFilters[band].setCoefficients(b0[band], b1[band], b2[band], a1[band],
                                       a2[band]);
  }

  localConfigIndex = index;
  localSequence = sequence;
}
