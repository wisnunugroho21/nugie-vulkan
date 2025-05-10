#pragma once

#include <glm/ext.hpp>
#include <glm/glm.hpp>

enum ToneMappingMode {
	ToneMapping_None = 0,
	ToneMapping_Reinhard = 1,
	ToneMapping_Uchimura = 2,
	ToneMapping_KhronosPBR = 3,
};

// Uchimura 2017, "HDR theory and practice"
// Math: https://www.desmos.com/calculator/gslcdxvipg
// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
float uchimura(float x, float P, float a, float m, float l, float c, float b);

float reinhard2(float v, float maxWhite);

// Khronos PBR Neutral Tone Mapper
// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/README.md#pbr-neutral-specification
// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/pbrNeutral.glsl
float PBRNeutralToneMapping(float color, float startCompression, float desaturation);
