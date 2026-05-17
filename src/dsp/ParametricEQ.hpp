#pragma once

#include "BiquadFilter.hpp"
#include "DspConfig.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

struct EQBand {
  float frequency = 1000.0f;
  float gainDb = 0.0f;
  float q = 1.0f;
  bool enabled = true;
};

class ParametricEQ {
public:
  static constexpr std::size_t maxBands = DspConfig::eqBandCount;

  void setBands(const std::vector<EQBand> &bands, float sampleRate);
  void process(float *left, float *right, uint32_t frames);
  void reset();
  float preampLinear() const;
  std::size_t activeBandCount() const;

private:
  struct AtomicCoefficients {
    std::atomic<float> b0 = 1.0f;
    std::atomic<float> b1 = 0.0f;
    std::atomic<float> b2 = 0.0f;
    std::atomic<float> a1 = 0.0f;
    std::atomic<float> a2 = 0.0f;
    std::atomic<bool> enabled = false;
  };

  struct EqConfig {
    std::array<AtomicCoefficients, maxBands> bands;
    std::atomic<float> preamp = 1.0f;
    std::atomic<std::size_t> activeBands = 0;
    std::atomic<uint32_t> sequence = 0;
  };

  void refreshCoefficientsIfNeeded();

private:
  std::array<EqConfig, 2> configs;
  std::array<BiquadFilter, maxBands> leftFilters;
  std::array<BiquadFilter, maxBands> rightFilters;
  std::array<bool, maxBands> localEnabled{};

  std::atomic<uint32_t> activeConfigIndex = 0;
  uint32_t localConfigIndex = 0;
  uint32_t localSequence = 0;
  std::atomic<std::size_t> activeBands = 0;
  std::atomic<float> outputPreamp = 1.0f;
};
