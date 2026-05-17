#pragma once

class BiquadFilter {
public:
  void setPeaking(float sampleRate, float frequency, float q, float gainDb);
  void setCoefficients(float b0, float b1, float b2, float a1, float a2);
  float process(float sample);
  void reset();

private:
  float b0 = 1.0f;
  float b1 = 0.0f;
  float b2 = 0.0f;
  float a1 = 0.0f;
  float a2 = 0.0f;
  float targetB0 = 1.0f;
  float targetB1 = 0.0f;
  float targetB2 = 0.0f;
  float targetA1 = 0.0f;
  float targetA2 = 0.0f;
  float z1 = 0.0f;
  float z2 = 0.0f;
};
