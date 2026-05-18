#pragma once

#include <array>
#include <cstddef>

namespace DspConfig {

inline constexpr std::array<float, 9> eqFrequencies{
    110.0f, 220.0f, 400.0f, 750.0f, 1500.0f,
    3000.0f, 6000.0f, 12000.0f, 16000.0f};
inline constexpr std::array<float, 9> eqDefaultQ{
    0.72f, 0.78f, 0.86f, 0.92f, 0.96f,
    0.92f, 0.82f, 0.70f, 0.62f};

inline constexpr std::size_t eqBandCount = eqFrequencies.size();

inline constexpr float pi = 3.14159265358979323846f;
inline constexpr float coefficientSmoothing = 0.0025f;
inline constexpr float parameterSmoothing = 0.0015f;
inline constexpr float minFilterFrequencyHz = 10.0f;
inline constexpr float minEditableEqFrequencyHz = 20.0f;
inline constexpr float maxEditableEqFrequencyHz = 20000.0f;
inline constexpr float maxFilterNyquistRatio = 0.45f;
inline constexpr float minEqQ = 0.1f;
inline constexpr float maxEqQ = 18.0f;
inline constexpr float minEqGainDb = -18.0f;
inline constexpr float maxEqGainDb = 18.0f;
inline constexpr float minEffectiveEqGainDb = -12.0f;
inline constexpr float maxEffectiveEqGainDb = 10.0f;
inline constexpr float maxTotalPositiveEqBoostDb = 28.0f;
inline constexpr float insignificantGainDb = 0.01f;
inline constexpr float maxAutoHeadroomDb = 4.0f;
inline constexpr float autoHeadroomFreeBoostDb = 8.0f;
inline constexpr float autoHeadroomTotalBoostRatio = 0.08f;
inline constexpr float autoHeadroomPeakBoostRatio = 0.12f;

inline constexpr float minProcessorGainLinear = 0.0f;
inline constexpr float maxProcessorGainLinear = 2.0f;
inline constexpr float minStereoWidth = 0.65f;
inline constexpr float maxStereoWidth = 1.85f;
inline constexpr float outputMin = -1.0f;
inline constexpr float outputMax = 1.0f;

inline constexpr float defaultCompressorThresholdDb = -10.0f;
inline constexpr float defaultCompressorRatio = 1.25f;
inline constexpr float defaultCompressorAttackMs = 8.0f;
inline constexpr float defaultCompressorReleaseMs = 160.0f;
inline constexpr float defaultLimiterCeilingDb = -0.2f;
inline constexpr float defaultLimiterCeilingLinear = 0.9772372f;

inline constexpr float minCompressorThresholdDb = -60.0f;
inline constexpr float maxCompressorThresholdDb = 0.0f;
inline constexpr float minCompressorRatio = 1.0f;
inline constexpr float maxCompressorRatio = 20.0f;
inline constexpr float minCompressorAttackMs = 0.1f;
inline constexpr float maxCompressorAttackMs = 200.0f;
inline constexpr float minCompressorReleaseMs = 1.0f;
inline constexpr float maxCompressorReleaseMs = 1000.0f;
inline constexpr float minLimiterCeilingDb = -24.0f;
inline constexpr float maxLimiterCeilingDb = -0.2f;
inline constexpr float limiterReleaseMs = 35.0f;
inline constexpr float tinySignal = 0.0000001f;
inline constexpr float denormalFloor = 0.000000000000000001f;

} // namespace DspConfig
