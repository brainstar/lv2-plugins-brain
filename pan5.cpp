#include <math.h>
#include <vector>
#include <lvtk/plugin.hpp>

#define PAN_URI "http://github.com/brainstar/lv2/pan5"
const uint32_t BUFFER_SIZE = 10000;

class Pan : public lvtk::Plugin<Pan> {
public:
	Pan(const lvtk::Args &args) : Plugin(args) {
		sample_rate = static_cast<float> (args.sample_rate);
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
		for (int i = 0; i < 5; i++) {
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
		// Define source count, fixed by plugin input size at compile time
		const int COUNT = 5;

		// Define angles
		if (pdist > 2 * r) pdist = r;
		float alpha = 2 * asin(pdist / (2.0 * r)); // Angle between two sources
		// float alpha0 = 0.f; // Initial angle of center [rad] (center = 0, right > 0)
		float alpha_p[COUNT]; // Angle of individual sources [rad] (center = 0, right > 0)

		// Calculate angles of individual sources
		// First for symmetrical setup...
		if (COUNT % 2 == 0) {
			for (int i = 0; i < COUNT / 2; i++) {
				alpha_p[(COUNT / 2) + i] = (0.5f + i) * alpha;
				alpha_p[(COUNT / 2) - (1 + i)] = -alpha_p[(COUNT / 2) + i];
			}
		} else {
			alpha_p[(COUNT - 1) / 2] = 0.f;
			for (int i = 1; i < (COUNT + 1) / 2; i++) {
				alpha_p[(COUNT - 1) / 2 + i] = alpha * i; 
				alpha_p[(COUNT - 1) / 2 - i] = -alpha_p[(COUNT - 1) / 2 + i];
			}
		}
		// ...then add angle displacement.
		for (int i = 0; i < COUNT; i++) alpha_p[i] += a0;

		double posx, posy, time_l, time_r;
		double dist_l[COUNT];
		double dist_r[COUNT];
		double attenuation = 1.0;

		for (int i = 0; i < COUNT; i++) {
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
		for (int i = 0; i < COUNT; i++) {
			attenuation_l[i] *= attenuation;
			attenuation_r[i] *= attenuation;
		}
	}

private:
	float* input[5] { 0, 0, 0, 0, 0 };
	float* output[2] { 0, 0 };
	float* radius = NULL;
	float* player_dist = NULL;
	float* ear_dist = NULL;
	float* alpha0 = NULL;

	float r_target = 0;
	float pdist_target = 0;
	float edist_target = 0;
	float a0_target = 0;
	float sample_rate;
	float v_air = 343.2;

	int samples_l[5], samples_r[5];
	float attenuation_l[5], attenuation_r[5];
	float buffer_l[BUFFER_SIZE];
	float buffer_r[BUFFER_SIZE];
	uint32_t buffer_ptr;
};

static const lvtk::Descriptor<Pan> pan (PAN_URI);