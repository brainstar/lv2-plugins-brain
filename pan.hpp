/*
 * Brain's Pan, a LV2 ensemble panner
 * Copyright (c) 2020 Christian Masser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the LICENSE file.
 */
 
//To enable DAZ
#include <pmmintrin.h>
//To enable FTZ
#include <xmmintrin.h>
#include <cstdint>
#include <math.h>
#include "triangularaverage.hpp"

class Pan {
public:
	Pan() {
		_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
		_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	}

	~Pan() {
		for (int i = 0; i < 2; i++) {
			delete[] avg[i];
			delete[] attenuation[i];
			delete[] delay[i];
			delete[] dist[i];
		}

		for (int ch = 0; ch < CHANNELS; ch++) {
			delete[] inputBuffer[ch];
		}
		
		delete[] delayBuffer;
		delete[] avg;
		delete[] attenuation;
		delete[] delay;
		delete[] dist;
		delete[] inputBuffer;
	}

	void init(int srate) {
		// Take values from ttl file
		// Max. signal path: max. radius + max. ear distance
		// Max. dealy = max. sig. path / v_air
		// Max. sample delay = max. delay / duration of single sample
		// Buffer size > max. sample delay
		// Here: double of max. sample delay
		BUFFER_SIZE = ((20.0 + 1.0) / v_air) / (1.0 / sample_rate) * 2;
		r_target = 5.;
		pdist_target = 1.;
		edist_target = 0.149;
		a0_target = 0.f;
		v_air = 343.2;

		avgBatchSize = 8;
		int batches;
		while (srate % avgBatchSize != 0) avgBatchSize /= 2;
		batches = (2 * srate) / avgBatchSize;

		delay = new int*[2];
		attenuation = new float*[2];
		avg = new TriangularAverage*[2];
		dist = new double*[2];

		for (int i = 0; i < 2; i++) {
			avg[i] = new TriangularAverage[CHANNELS];
			attenuation[i] = new float[CHANNELS];
			delay[i] = new int[CHANNELS];
			dist[i] = new double[CHANNELS];

			for (int j = 0; j < CHANNELS; j++) {
				avg[i][j].init(batches);
				avg[i][j].setWindowSize(batches / 2);
				attenuation[i][j] = 1.f;
				delay[i][j] = 0;
			}
		}

		input = new float*[CHANNELS];
		inputBuffer = new float*[CHANNELS];
		delayBuffer = new float[CHANNELS];
		for (int ch = 0; ch < CHANNELS; ch++) {
			input[ch] = nullptr;
			delayBuffer[ch] = 0.f;
			inputBuffer[ch] = new float[BUFFER_SIZE];
			for (int i = 0; i < BUFFER_SIZE; i++) {
				inputBuffer[ch][i] = 0.f;
			}
		}
		
		generalBufferPointer = 0;
		timer = 0;
		timerOverrun = (batches / 2 + 2) * avgBatchSize;
		useAverage = true;
	}
	
	void connect_portBase(uint32_t port, void* data) {
		if (port == 0) {
			radius = (float*) data;
		}
		else if (port == 1) {
			player_dist = (float*) data;
		}
		else if (port == 2) {
			ear_dist = (float*) data;
		}
		else if (port == 3) {
			alpha0 = (float*) data;
		}
		else if (port == 4) {
			window_size = (float*) data;
		}
		else if (port == 5) {
			relative_delays = (float*) data;
		}
		else if (port == 6) {
			output[0] = (float*) data;
		}
		else if (port == 7) {
			output[1] = (float*) data;
		}
		else if (port >= 8 && port < (8 + CHANNELS)) {
			input[port - 8] = (float*) data;
		}
	}

	void activateBase() {
		// Clean buffer
		for (int j = 0; j < CHANNELS; j++) {
			for (int i = 0; i < BUFFER_SIZE; i++) {
				inputBuffer[j][i] = 0;
			}
			for (int i = 0; i < 2; i++) {
				avg[i][j].clean();
			}
		}
		generalBufferPointer = 0;
		timer = 0;
		useAverage = true;
	} 

	void deactivateBase() {
	}

	float getInputValue(int ch, int offset) {
		// Transform relative offset -> position in array
		offset += generalBufferPointer;

		// Scale into correct values
		while (offset < 0) offset += BUFFER_SIZE;
		while (offset >= BUFFER_SIZE) offset -= BUFFER_SIZE;

		return inputBuffer[ch][offset];
	}

	float getInterpolatedValue(int ch, float offset) {
		float position;
		int index1, index2;
		float weight1, weight2;

		// Determine correct address in ring buffer
		position = offset + generalBufferPointer;
		while (position < 0) position += BUFFER_SIZE;

		// Determine corresponding indices and weights.
		index1 = (int) position;
		weight2 = position - (float) index1;
		index1 %= BUFFER_SIZE;
		index2 = (index1 + 1) % BUFFER_SIZE;
		weight1 = 1.0 - weight2;

		return inputBuffer[ch][index1] * weight1 + inputBuffer[ch][index2] * weight2;
	}

	void runBase(uint32_t nframes) {
		// Update data if necessary
		if (*window_size != window_target) {
			window_target = *window_size;
			for (int i = 0; i < 2; i++) {
				for (int ch = 0; ch < CHANNELS; ch++) {
					avg[i][ch].setWindowSize(window_target * sample_rate / avgBatchSize);
				}
			}
			timer = 0;
			timerOverrun = (avg[0][0].getWindowSize() + 2) * avgBatchSize;
			useAverage = true;
		}
		if (*radius != r_target
			|| *player_dist != pdist_target
			|| *ear_dist != edist_target
			|| *alpha0 != a0_target
			|| *relative_delays != rel_delay_target) {
			update_data(*radius, *player_dist, *ear_dist, *alpha0, *relative_delays);
			r_target = *radius;
			pdist_target = *player_dist;
			edist_target = *ear_dist;
			a0_target = *alpha0;
			rel_delay_target = *relative_delays;
		}

		if (useAverage) {
			// TODO: What if nframes % batchsize != 0??? (Should not be the case, but Murphy)
			for (int i = 0; i < CHANNELS; i++) {
				avg[0][i].pushData(delay[0][i], nframes / avgBatchSize);
				avg[1][i].pushData(delay[1][i], nframes / avgBatchSize);
			}
			timer += nframes;
		}

		// Step 1: Buffer input
		for (int ch = 0; ch < CHANNELS; ch++) {
			// Is there an overflow?
			if (generalBufferPointer + nframes <= BUFFER_SIZE) {
				// If not: simply copy all the elements
				for (int i = 0; i < nframes; i++) inputBuffer[ch][generalBufferPointer + i] = input[ch][i];
			} else {
				int sizeLeft = BUFFER_SIZE - generalBufferPointer;
				for (int i = 0; i < sizeLeft; i++) inputBuffer[ch][generalBufferPointer + i] = input[ch][i];
				int framesLeft = nframes - sizeLeft;
				for (int i = 0; i < framesLeft; i++) inputBuffer[ch][i] = input[ch][sizeLeft + i];
			}
		}

		// Step 2: Output
		int batches;
		float value;
		if (!useAverage) {
			for (int i = 0; i < 2; i++) {
				for (int f = 0; f < nframes; f++) {
					value = 0.f;
					// Get every frame from the right buffer
					for (int ch = 0; ch < CHANNELS; ch++) {
						value += (getInputValue(ch, f - delay[i][ch]) * attenuation[i][ch]);
					}
					output[i][f] = value;
				}
			}
		} else {
			for (int i = 0; i < 2; i++) {
				batches = nframes / avgBatchSize;
				for (int b = 0; b < batches; b++) {
					// Fill delay buffer
					for (int ch = 0; ch < CHANNELS; ch++) delayBuffer[ch] = avg[i][ch].popData();
					for (int f = 0; f < avgBatchSize; f++) {
						value = 0.f;
						// Get every frame from the right buffer
						for (int ch = 0; ch < CHANNELS; ch++) {
							value += (getInterpolatedValue(ch, (f + b * avgBatchSize) - delayBuffer[ch]) * attenuation[i][ch]);
						}
						output[i][f + b * avgBatchSize] = value;
					}
				}
			}
			if (timer > timerOverrun) {
				useAverage = false;
				timer = 0;
			}
		}
		generalBufferPointer += nframes;
		if (generalBufferPointer >= BUFFER_SIZE) generalBufferPointer -= BUFFER_SIZE;
	}

	void update_data(float r, float pdist, float eardist, float a0, float rel_delay) {
		if (r == 0) r = 0.01f;
		// Define angles
		// Angle between two sources
		float alpha = (pdist > 2 * r) ? M_PI : (2 * asin(pdist / (2.0 * r)));
		// float alpha0 = 0.f; // Initial angle of center [rad] (center = 0, right > 0)
		float alpha_p[CHANNELS]; // Angle of individual sources [rad] (center = 0, right > 0)

		// Calculate angles of individual sources
		// First for symmetrical setup...
		if (CHANNELS % 2 == 0) {
			for (int i = 0; i < CHANNELS / 2; i++) {
				alpha_p[(CHANNELS / 2) + i] = (0.5f + i) * alpha;
				alpha_p[(CHANNELS / 2) - (1 + i)] = -alpha_p[(CHANNELS / 2) + i];
			}
		} else {
			alpha_p[CHANNELS / 2] = 0.f;
			for (int i = 1; i < CHANNELS / 2; i++) {
				alpha_p[CHANNELS / 2 + i] = alpha * i; 
				alpha_p[CHANNELS / 2 - i] = -alpha_p[CHANNELS / 2 + i];
			}
		}
		// ...then add angle displacement. (You have to convert alpha from degrees to radiant!)
		for (int i = 0; i < CHANNELS; i++) alpha_p[i] += (a0 / 180.f * M_PI);

		double posx, posy, time_l, time_r;
		double att = 1.0;

		for (int i = 0; i < CHANNELS; i++) {
			// Calculate position of source
			posx = r * sin(alpha_p[i]);
			posy = r * cos(alpha_p[i]);

			// Calculate distance to listener
			dist[0][i] = sqrt(pow((posx + eardist / 2.0), 2) + pow(posy, 2));
			dist[1][i] = sqrt(pow((posx - eardist / 2.0), 2) + pow(posy, 2));

			// Calculate attenuation and build product over attenuation
			attenuation[0][i] = r / dist[0][i];
			attenuation[1][i] = r / dist[1][i];
			att *= attenuation[0][i];
			att *= attenuation[1][i];

			// Calculate sample delay
			time_l = dist[0][i] / v_air;
			time_r = dist[1][i] / v_air;
			delay[0][i] = (int) round(time_l / (1.0 / sample_rate));
			delay[1][i] = (int) round(time_r / (1.0 / sample_rate));
		}

		if (rel_delay > 0.5) {
			// Reduce to relative delay between sources only
			int min = delay[0][0];
			for (int i = 0; i < 2; i++) {
				for (int ch = 0; ch < CHANNELS; ch++) {
					if (delay[i][ch] < min) min = delay[i][ch];
				}
			}
			for (int i = 0; i < 2; i++) {
				for (int ch = 0; ch < CHANNELS; ch++) {
					delay[i][ch] -= min;
				}
			}
		}

		// Normalize attenuation
		att = 1.f / att;
		for (int i = 0; i < CHANNELS; i++) {
			attenuation[0][i] *= att;
			attenuation[1][i] *= att;
		}

		timer = 0;
		useAverage = true;
	}

protected:
	int BUFFER_SIZE;
	int CHANNELS;

	float** input;
	float* output[2] { 0, 0 };
	float* radius = nullptr;
	float* player_dist = nullptr;
	float* ear_dist = nullptr;
	float* alpha0 = nullptr;
	float* relative_delays = nullptr;
	float* window_size = nullptr;

	float r_target = 0;
	float pdist_target = 0;
	float edist_target = 0;
	float a0_target = 0;
	float sample_rate;
	float v_air = 343.2;
	float rel_delay_target = 0;
	float window_target = 1.0;

	int** delay;
	float** attenuation;
	TriangularAverage** avg;

	double** dist;

	float** inputBuffer;

	float *delayBuffer;

	int generalBufferPointer;

	int avgBatchSize;
	int timer, timerOverrun;
	bool useAverage;
};