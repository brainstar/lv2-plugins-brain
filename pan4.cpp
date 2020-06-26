#include <math.h>
#include <vector>
#include <lvtk/plugin.hpp>

#define PAN_URI "http://github.com/brainstar/lv2/pan4"
const int CHANNELS = 4;

class SampleAverager
{
public:
	SampleAverager() {}
	~SampleAverager() {}

	void initialize(int framerate, int divider) {
		// Set Framerate, used for extracting the values
		frate = framerate;
		// Only save every x-th value, interpolation on extraction
		this->divider = divider;
		// size of divider, should divide framerate;
		size = frate / divider;

		samples_data.resize(size, 0);
		samples_sum.resize(size, 0);
		samples_sum_reduced.resize(size, 0.f);
		rest = 0;
	}

	void pushData(int value, int nframes) {
		// Step 1: First check, if there are enough elements to work with
		if (nframes + rest < divider) {
			rest += nframes;
			return;
		}

		// Step 2: If there's a rest, handle it before
		if (rest) {
			// (nframes + rest) - divider
			nframes -= (divider - rest);
			samples_sum[fillPtr] = samples_sum[fillPtr == 0 ? (size - 1) : fillPtr]
				- samples_data[fillPtr] + value;
			samples_data[fillPtr] = value;
			samples_sum_reduced[fillPtr] = (float) samples_sum[fillPtr] / size;
			fillPtr++;
			if (fillPtr == size) fillPtr = 0;
		}

		// Step 3
		// Caculate amount of samples and resulting rest
		int fullSamplesCount = nframes / divider;
		rest = nframes % divider;

		// No full sapmles? Abort.
		if (!fullSamplesCount) return;

		// 3.a) First check if fillPtr == 0
		if (fillPtr == 0) {
			samples_sum[0] = samples_sum[size - 1]
				- samples_data[0] + value;
			samples_data[0] = value;
			samples_sum_reduced[0] = (float) samples_sum[0] / size;
			fillPtr = 1;
			fullSamplesCount--;
		}

		// 3.b) Single or double pass?
		if (fillPtr + fullSamplesCount < size) {
			// 3.c1) Single pass
			for (int i = fillPtr; i < fillPtr + fullSamplesCount; i++) {
				samples_sum[i] = samples_sum[i - 1]
					- samples_data[i] + value;
				samples_data[i] = value;
				samples_sum_reduced[i] = (float) samples_sum[i] / size;
			}
			fillPtr += fullSamplesCount;
		} else {
			// 3.c2) Double pass: TODO
			// Calculate frame count for second pass
			fullSamplesCount -= (size - fillPtr);
			// Fill in frames from [fillPtr, size[
			for (int i = fillPtr; i < size; i++) {
				samples_sum[i] = samples_sum[i - 1]
					- samples_data[i] + value;
				samples_data[i] = value;
				samples_sum_reduced[i] = (float) samples_sum[i] / size;
			}
			// Fill in the 0
			samples_sum[0] = samples_sum[size - 1]
				- samples_data[0] + value;
			samples_data[0] = value;
			samples_sum_reduced[0] = (float) samples_sum[0] / size;
			fillPtr = 1;

			// Fill in frames from [1, fullSamplesCount[
			for (int i = fillPtr; i < fullSamplesCount; i++) {
				samples_sum[i] = samples_sum[i - 1]
					- samples_data[i] + value;
				samples_data[i] = value;
				samples_sum_reduced[i] = (float) samples_sum[i] / size;
			}

			// Reset fillPtr
			fillPtr = fullSamplesCount;
		}
	}

	float getData(int &batchsize) {
		if (readPtr >= size) readPtr -= size;
		batchsize = size;
		return samples_sum_reduced[readPtr++];
	}

private:
	std::vector<int> samples_data;
	std::vector<long> samples_sum;
	std::vector<float> samples_sum_reduced;
	int frate, divider, size;
	int rest = 0;
	int fillPtr = 0, readPtr = 0;
};

class Pan : public lvtk::Plugin<Pan> {
public:
	Pan(const lvtk::Args &args) : Plugin(args) {
		sample_rate = static_cast<float> (args.sample_rate);
		for (int i = 0; i < 4; i++) {
			samples_l[i] = samples_r[i] = 0;
			attenuation_l[i] = attenuation_r[i] = 1.f;
		}
		// Take values from ttl file
		// Max. signal path: max. radius + max. ear distance
		// Max. dealy = max. sig. path / v_air
		// Max. sample delay = max. delay / duration of single sample
		// Buffer size > max. sample delay
		// Here: double of max. sample delay
		BUFFER_SIZE = ((20.0 + 1.0) / v_air) / (1.0 / sample_rate) * 2;
		buffer_l.resize(BUFFER_SIZE);
		buffer_r.resize(BUFFER_SIZE);
		avg.resize(2);
		for (int i = 0; i < 2; i++) {
			avg[i].resize(CHANNELS);
			for (int j = 0; j < CHANNELS; j++) {
				avg[i][j].initialize(sample_rate, 4);
			}
		}
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
			input[0] = (float*) data;
		}
		else if (port == 5) {
			input[1] = (float*) data;
		}
		else if (port == 6) {
			input[2] = (float*) data;
		}
		else if (port == 7) {
			input[3] = (float*) data;
		}
		else if (port == 8) {
			output[0] = (float*) data;
		}
		else if (port == 9) {
			output[1] = (float*) data;
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

		for (int i = 0; i < CHANNELS; i++) {
			avg[0][i].pushData(samples_l[i], nframes);
			avg[1][i].pushData(samples_r[i], nframes);
		}

		// buffer_ptr <=> frame 0
		// Write data to buffer
		// Iterate through sources
		uint32_t offset = 0;
		int passes = 1;
		uint32_t buffer_frame_start[2];
		uint32_t input_frame_start[2];
		uint32_t input_frame_size[2];
		for (int i = 0; i < 4; i++) {
			int dummy;
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
			int dummycnt = 0;
			for (int p = 0; p < passes; p++) {
				for (int f = 0; f < input_frame_size[p]; f++) {
					if (dummycnt == dummy) {
						avg[0][i].getData(dummy);
						dummycnt = 0;
					}
					buffer_l[buffer_frame_start[p] + f] += (input[i][input_frame_start[p] + f]
						* attenuation_l[i]);
					dummycnt++;
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
			dummycnt = 0;
			for (int p = 0; p < passes; p++) {
				for (int f = 0; f < input_frame_size[p]; f++) {
					if (dummycnt == dummy) {
						avg[1][i].getData(dummy);
						dummycnt = 0;
					}
					buffer_r[buffer_frame_start[p] + f] += (input[i][input_frame_start[p] + f]
						* attenuation_r[i]);
					dummy++;
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
		const int COUNT = 4;

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
				alpha_p[(COUNT + 1) / 2 + i] = alpha * i; 
				alpha_p[(COUNT + 1) / 2 - i] = -alpha_p[(COUNT - 1) / 2 + i];
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
	float* input[4] { 0, 0, 0, 0 };
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

	int samples_l[CHANNELS], samples_r[CHANNELS];
	float attenuation_l[CHANNELS], attenuation_r[CHANNELS];
	std::vector<float> buffer_l;
	std::vector<float> buffer_r;
	uint32_t buffer_ptr;
	int BUFFER_SIZE;
	std::vector<std::vector<SampleAverager> > avg;
};

static const lvtk::Descriptor<Pan> pan (PAN_URI);