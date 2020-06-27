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

#include <math.h>
#include <vector>
#include <lvtk/plugin.hpp>

#define PAN_URI "http://github.com/brainstar/lv2/pan9"
const int CHANNELS = 9;

class Pan : public lvtk::Plugin<Pan> {
public:
	Pan(const lvtk::Args &args) : Plugin(args) {
		sample_rate = static_cast<float> (args.sample_rate);
		// Take values from ttl file
		// Max. signal path: max. radius + max. ear distance
		// Max. dealy = max. sig. path / v_air
		// Max. sample delay = max. delay / duration of single sample
		// Buffer size > max. sample delay
		// Here: double of max. sample delay
		BUFFER_SIZE = ((20.0 + 1.0) / v_air) / (1.0 / sample_rate) * 2;
		buffer_l.resize(BUFFER_SIZE);
		buffer_r.resize(BUFFER_SIZE);
	}

	void connect_port(uint32_t port, void* data) {
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
			output[0] = (float*) data;
		}
		else if (port == 5) {
			output[1] = (float*) data;
		}
		else if (port == 6) {
			input[0] = (float*) data;
		}
		else if (port == 7) {
			input[1] = (float*) data;
		}
		else if (port == 8) {
			input[2] = (float*) data;
		}
		else if (port == 9) {
			input[3] = (float*) data;
		}
		else if (port == 10) {
			input[4] = (float*) data;
		}
		else if (port == 11) {
			input[5] = (float*) data;
		}
		else if (port == 12) {
			input[6] = (float*) data;
		}
		else if (port == 13) {
			input[7] = (float*) data;
		}
		else if (port == 14) {
			input[8] = (float*) data;
		}
	}

	void activate() {
		for (int i = 0; i < BUFFER_SIZE; i++) {
			buffer_l[i] = 0.0;
			buffer_r[i] = 0.0;
		}
		buffer_ptr = 0;
	}

	void deactivate() {
	}

	void run(uint32_t nframes) {
		// Update data if necessary
		if (*radius != r_target
			|| *player_dist != pdist_target
			|| *ear_dist != edist_target
			|| *alpha0 != a0_target) {
			update_data(*radius, *player_dist, *ear_dist, *alpha0);
			r_target = *radius;
			pdist_target = *player_dist;
			edist_target = *ear_dist;
			a0_target = *alpha0;
		}

		// buffer_ptr <=> frame 0
		// Write data to buffer
		// Iterate through sources
		uint32_t offset = 0;
		int passes = 1;
		uint32_t buffer_frame_start[2];
		uint32_t input_frame_start[2];
		uint32_t input_frame_size[2];
		for (int i = 0; i < CHANNELS; i++) {
			// Left side:
			// Determine number of passes:
			offset = buffer_ptr + samples_l[i];
			if (offset > BUFFER_SIZE) {
				// Single pass, total overflow
				passes = 1;
				buffer_frame_start[0] = (buffer_ptr + samples_l[i]) - BUFFER_SIZE;
				input_frame_start[0] = 0;
				input_frame_size[0] = nframes;
			} else if (offset + nframes > BUFFER_SIZE) {
				// Dual pass, partial overflow
				passes = 2;
				buffer_frame_start[0] = offset;
				input_frame_start[0] = 0;
				input_frame_size[0] = BUFFER_SIZE - offset;
				buffer_frame_start[1] = 0;
				input_frame_start[1] = input_frame_size[0];
				input_frame_size[1] = nframes - input_frame_size[0];
			} else {
				// Single pass, no overflow
				passes = 1;
				buffer_frame_start[0] = offset;
				input_frame_start[0] = 0;
				input_frame_size[0] = nframes;
			}

			// Make passes
			for (int p = 0; p < passes; p++) {
				for (int f = 0; f < input_frame_size[p]; f++) {
					buffer_l[buffer_frame_start[p] + f] += (input[i][input_frame_start[p] + f]
						* attenuation_l[i]);
				}
			}

			// Right side:
			// Determine number of passes:
			offset = buffer_ptr + samples_r[i];
			if (offset > BUFFER_SIZE) {
				// Single pass, total overflow
				passes = 1;
				buffer_frame_start[0] = (buffer_ptr + samples_r[i]) - BUFFER_SIZE;
				input_frame_start[0] = 0;
				input_frame_size[0] = nframes;
			} else if (offset + nframes > BUFFER_SIZE) {
				// Dual pass, partial overflow
				passes = 2;
				buffer_frame_start[0] = offset;
				input_frame_start[0] = 0;
				input_frame_size[0] = BUFFER_SIZE - offset;
				buffer_frame_start[1] = 0;
				input_frame_start[1] = input_frame_size[0];
				input_frame_size[1] = nframes - input_frame_size[0];
			} else {
				// Single pass, no overflow
				passes = 1;
				buffer_frame_start[0] = offset;
				input_frame_start[0] = 0;
				input_frame_size[0] = nframes;
			}

			// Make passes
			for (int p = 0; p < passes; p++) {
				for (int f = 0; f < input_frame_size[p]; f++) {
					buffer_r[buffer_frame_start[p] + f] += (input[i][input_frame_start[p] + f]
						* attenuation_r[i]);
				}
			}
		}

		// Output buffer to stream
		if (buffer_ptr + nframes > BUFFER_SIZE) {
			uint32_t frames_pass_1 = BUFFER_SIZE - buffer_ptr;
			uint32_t frames_pass_2 = nframes - frames_pass_1;
			// Pass 1
			for (int f = 0; f < frames_pass_1; f++) {
				output[0][f] = buffer_l[buffer_ptr];
				output[1][f] = buffer_r[buffer_ptr];
				buffer_l[buffer_ptr] = 0.f;
				buffer_r[buffer_ptr] = 0.f;
				buffer_ptr++;
			}
			// Pass 2
			buffer_ptr = 0;
			for (int f = 0; f < frames_pass_2; f++) {
				output[0][frames_pass_1 + f] = buffer_l[buffer_ptr];
				output[1][frames_pass_1 + f] = buffer_r[buffer_ptr];
				buffer_l[buffer_ptr] = 0.f;
				buffer_r[buffer_ptr] = 0.f;
				buffer_ptr++;
			}
		} else {
			// Single pass
			for (uint32_t f = 0; f < nframes; f++) {
				output[0][f] = buffer_l[buffer_ptr];
				output[1][f] = buffer_r[buffer_ptr];
				buffer_l[buffer_ptr] = 0.f;
				buffer_r[buffer_ptr] = 0.f;
				buffer_ptr++;
			}
		}
	}

	void update_data(float r, float pdist, float eardist, float a0) {
		// Safety check 1: player distance can't be > 2*r
		float alpha;
		// Define angles
		if (pdist > 2 * r) {
			pdist = 2 * r;
			alpha = 2 * M_PI;
		} else {
			alpha = 2 * asin(pdist / (2.0 * r)); // Angle between two sources
		}
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
			alpha_p[(CHANNELS - 1) / 2] = 0.f;
			for (int i = 1; i < (CHANNELS + 1) / 2; i++) {
				alpha_p[(CHANNELS - 1) / 2 + i] = alpha * i; 
				alpha_p[(CHANNELS - 1) / 2 - i] = -alpha_p[(CHANNELS - 1) / 2 + i];
			}
		}
		// ...then add angle displacement.
		for (int i = 0; i < CHANNELS; i++) alpha_p[i] += a0;

		double posx, posy, time_l, time_r;
		double dist_l[CHANNELS];
		double dist_r[CHANNELS];
		double attenuation = 1.0;

		for (int i = 0; i < CHANNELS; i++) {
			// Calculate position of source
			posx = r * sin(alpha_p[i]);
			posy = r * cos(alpha_p[i]);

			// Calculate distance to listener
			dist_l[i] = sqrt(pow((posx + eardist / 2.0), 2) + pow(posy, 2));
			dist_r[i] = sqrt(pow((posx - eardist / 2.0), 2) + pow(posy, 2));

			// Calculate attenuation and build product over attenuation
			attenuation_l[i] = r / dist_l[i];
			attenuation_r[i] = r / dist_r[i];
			attenuation *= attenuation_l[i];
			attenuation *= attenuation_r[i];

			// Calculate sample delay
			time_l = dist_l[i] / v_air;
			time_r = dist_r[i] / v_air;
			samples_l[i] = (int) round(time_l / (1.0 / sample_rate));
			samples_r[i] = (int) round(time_r / (1.0 / sample_rate));
		}

		// Normalize attenuation
		attenuation = 1.f / attenuation;
		for (int i = 0; i < CHANNELS; i++) {
			attenuation_l[i] *= attenuation;
			attenuation_r[i] *= attenuation;
		}
	}

	static bool getInterpolatedFrame(float* source, int nframes, float i, float &sample) {
		if (i > (nframes - 1)) return false;
		if (i == (nframes - 1)) {
			sample = source[nframes - 1];
			return true;
		}

		int baseIndex = i;
		float scalar = i - baseIndex;
		sample = source[baseIndex] * scalar + source[baseIndex + 1] * (1.0 - scalar);
		return true;
	}

private:
	// Adapt input initialization for channel count change
	float* input[CHANNELS] { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	float* output[2] { 0, 0 };
	float* radius = NULL;
	float* player_dist = NULL;
	float* ear_dist = NULL;
	float* alpha0 = NULL;

	float r_target = 5;
	float pdist_target = 1;
	float edist_target = 0.149;
	float a0_target = 0;
	float sample_rate;
	float v_air = 343.2; // sonic velocity in air [m/s]

	int samples_l[CHANNELS], samples_r[CHANNELS];
	float attenuation_l[CHANNELS], attenuation_r[CHANNELS];
	std::vector<float> buffer_l;
	std::vector<float> buffer_r;
	uint32_t buffer_ptr;
	int BUFFER_SIZE;
};

static const lvtk::Descriptor<Pan> pan (PAN_URI);