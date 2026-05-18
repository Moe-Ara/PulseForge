#include "BiquadFilter.hpp"

#include "DspConfig.hpp"

#include <algorithm>
#include <cmath>

namespace {

bool valid(float value) {
  return std::isfinite(value);
}

} // namespace

void BiquadFilter::setPeaking(float sampleRate, float frequency, float q,
                              float gainDb) {
  sampleRate = std::max(sampleRate, 1.0f);
  frequency = std::clamp(frequency, DspConfig::minFilterFrequencyHz,
                         sampleRate * DspConfig::maxFilterNyquistRatio);
  q = std::clamp(q, DspConfig::minEqQ, DspConfig::maxEqQ);
  gainDb =
      std::clamp(gainDb, DspConfig::minEqGainDb, DspConfig::maxEqGainDb);

  const float a = std::pow(10.0f, gainDb / 40.0f);
  const float omega = 2.0f * DspConfig::pi * frequency / sampleRate;
  const float sinOmega = std::sin(omega);
  const float cosOmega = std::cos(omega);
  const float alpha = sinOmega / (2.0f * q);

  const float rawB0 = 1.0f + alpha * a;
  const float rawB1 = -2.0f * cosOmega;
  const float rawB2 = 1.0f - alpha * a;
  const float rawA0 = 1.0f + alpha / a;
  const float rawA1 = -2.0f * cosOmega;
  const float rawA2 = 1.0f - alpha / a;

  if (!valid(rawA0) || std::abs(rawA0) < 0.000001f) {
    setCoefficients(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    return;
  }

  setCoefficients(rawB0 / rawA0, rawB1 / rawA0, rawB2 / rawA0,
                  rawA1 / rawA0, rawA2 / rawA0);
}

void BiquadFilter::setCoefficients(float nextB0, float nextB1, float nextB2,
                                   float nextA1, float nextA2) {
  if (!valid(nextB0) || !valid(nextB1) || !valid(nextB2) || !valid(nextA1) ||
      !valid(nextA2)) {
    targetB0 = 1.0f;
    targetB1 = 0.0f;
    targetB2 = 0.0f;
    targetA1 = 0.0f;
    targetA2 = 0.0f;
    return;
  }

  targetB0 = nextB0;
  targetB1 = nextB1;
  targetB2 = nextB2;
  targetA1 = nextA1;
  targetA2 = nextA2;
}

float BiquadFilter::process(float sample) {
  if (!valid(sample)) {
    sample = 0.0f;
  }

  b0 += (targetB0 - b0) * DspConfig::coefficientSmoothing;
  b1 += (targetB1 - b1) * DspConfig::coefficientSmoothing;
  b2 += (targetB2 - b2) * DspConfig::coefficientSmoothing;
  a1 += (targetA1 - a1) * DspConfig::coefficientSmoothing;
  a2 += (targetA2 - a2) * DspConfig::coefficientSmoothing;

  const float output = b0 * sample + z1;
  z1 = b1 * sample - a1 * output + z2;
  z2 = b2 * sample - a2 * output;

  if (std::abs(z1) < DspConfig::denormalFloor) {
    z1 = 0.0f;
  }
  if (std::abs(z2) < DspConfig::denormalFloor) {
    z2 = 0.0f;
  }

  if (!valid(output) || !valid(z1) || !valid(z2)) {
    reset();
    return 0.0f;
  }

  return output;
}

void BiquadFilter::reset() {
  z1 = 0.0f;
  z2 = 0.0f;
}
