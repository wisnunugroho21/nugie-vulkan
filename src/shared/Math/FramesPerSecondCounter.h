#pragma once

#include <assert.h>
#include <stdio.h>

class FramesPerSecondCounter {
public:
	explicit FramesPerSecondCounter(float avgInterval = 0.5f);

	bool tick(float deltaSeconds, bool frameRendered = true);
	inline float getFPS() const { return currentFPS_; }

	bool printFPS_ = true;

public:
	float avgInterval_ = 0.5f;
	unsigned int numFrames_ = 0;
	double accumulatedTime_ = 0;
	float currentFPS_ = 0.0f;
};
