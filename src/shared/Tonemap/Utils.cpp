#include "Utils.h"

float uchimura(float x, float P, float a, float m, float l, float c, float b) {
	float l0 = ((P - m) * l) / a;
	float L0 = m - m / a;
	float L1 = m + (1.0f - m) / a;
	float S0 = m + l0;
	float S1 = m + a * l0;
	float C2 = (a * P) / (P - S1);
	float CP = -C2 / P;

	float w0 = float(1.0f - glm::smoothstep(0.0f, m, x));
	float w2 = float(glm::step(m + l0, x));
	float w1 = float(1.0f - w0 - w2);

	float T = float(m * pow(x / m, float(c)) + b);
	float S = float(P - (P - S1) * exp(CP * (x - S0)));
	float L = float(m + a * (x - m));

	return T * w0 + L * w1 + S * w2;
}

float reinhard2(float v, float maxWhite) {
	return v * (1.0f + (v / (maxWhite * maxWhite))) / (1.0f + v);
}

float PBRNeutralToneMapping(float color, float startCompression, float desaturation) {
	startCompression -= 0.04f;

	float x = color;
	float offset = x < 0.08f ? x - 6.25f * x * x : 0.04f;
	color -= offset;

	float peak = color;
	if (peak < startCompression)
		return color;

	const float d = 1. - startCompression;
	float newPeak = 1. - d * d / (peak + d - startCompression);
	color *= newPeak / peak;

	float g = 1.0f - 1.0f / (desaturation * (peak - newPeak) + 1.0f);
	return glm::mix(color, newPeak, g);
}