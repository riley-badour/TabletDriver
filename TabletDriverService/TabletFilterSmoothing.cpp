#include "stdafx.h"
#include "TabletFilterSmoothing.h"

#define LOG_MODULE "Smoothing"
#include "Logger.h"

//
// Constructor
//
TabletFilterSmoothing::TabletFilterSmoothing() {
	latency = 2.0;
	weight = 1.000;
	threshold = 0.9;
	lastTargetTime = chrono::high_resolution_clock::now();
}


//
// Destructor
//
TabletFilterSmoothing::~TabletFilterSmoothing() {
}


//
// TabletFilter methods
//

// Set target position
void TabletFilterSmoothing::SetTarget(TabletState *tabletState) {
	target.Set(tabletState->position);
	memcpy(&outputState, tabletState, sizeof(TabletState));

	// Reset output position if last tablet state update is older than 100 milliseconds.
	double timeDelta = (tabletState->time - lastTargetTime).count() / 1000000.0;
	if(timeDelta > 100.0) {
		outputPosition.Set(tabletState->position);
	}

	lastTargetTime = tabletState->time;
}

//
// Filter timer interval changed
//
void TabletFilterSmoothing::OnTimerIntervalChange(double oldInterval, double newInterval) {
	timerInterval = newInterval;
	SetLatency(latency);
}

// Update
void TabletFilterSmoothing::Update() {

	double deltaX, deltaY, distance;

	deltaX = target.x - outputPosition.x;
	deltaY = target.y - outputPosition.y;
	distance = sqrt(deltaX*deltaX + deltaY * deltaY);

	double timeDelta = (chrono::high_resolution_clock::now() - outputState.time).count() / 1000000.0;

	// Distance large enough?
	if(distance > 0.01) {

		// Move output position coordinates if last tablet update time is newer than 100 milliseconds.
		if(timeDelta < 100.0) {
			outputPosition.x += deltaX * weight;
			outputPosition.y += deltaY * weight;
		}

		// Debug message
		if(logger.debugEnabled) {
			LOG_DEBUG("TX=%0.2f TY=%0.2f OX=%0.2f OY=%0.2f DX=%0.2f DY=%0.2f D=%0.2f W=%0.3f\n",
				target.x,
				target.y,
				outputPosition.x,
				outputPosition.y,
				deltaX,
				deltaY,
				distance,
				weight
			);
		}

	}
	// Too short distance -> set output values as target values
	else {
		outputPosition.x = target.x;
		outputPosition.y = target.y;
	}

	outputState.position.Set(outputPosition);

}

//
// Calculate filter latency
//
double TabletFilterSmoothing::GetLatency(double filterWeight, double interval, double threshold) {
	double target = 1 - threshold;
	double stepCount = -log(1 / target) / log(1 - filterWeight);
	return stepCount * interval;
}
double TabletFilterSmoothing::GetLatency(double filterWeight) {
	return this->GetLatency(filterWeight, timerInterval, threshold);
}
double TabletFilterSmoothing::GetLatency() {
	return this->GetLatency(weight, timerInterval, threshold);
}


//
// Calculate filter weight
//
double TabletFilterSmoothing::GetWeight(double latency, double interval, double threshold) {
	double stepCount = latency / interval;
	double target = 1 - threshold;
	return 1 - 1 / pow(1 / target, 1 / stepCount);
}
double TabletFilterSmoothing::GetWeight(double latency) {
	return this->GetWeight(latency, this->timerInterval, this->threshold);
}

// Set Latency
void TabletFilterSmoothing::SetLatency(double latency) {
	this->weight = GetWeight(latency);
	this->latency = latency;
}


