//To enable DAZ
#include <pmmintrin.h>
//To enable FTZ
#include <xmmintrin.h>

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

	void clean() {
		for (int i = 0; i < size; i++) {
			samples_data[i] = 0;
			samples_sum[i] = 0;
			samples_sum_reduced[i] = 0;
		}
		fillPtr = 0;
		readPtr = 0;
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
		// Caculate amount of delay and resulting rest
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

	float getData() {
		if (readPtr >= size) readPtr -= size;
		return samples_sum_reduced[readPtr++];
	}

	int getBatchSize() {
		return divider;
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
		_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
		_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
		sample_rate = static_cast<float> (args.sample_rate);

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

		delay.resize(2);
		attenuation.resize(2);
		avg.resize(2);

		for (int i = 0; i < 2; i++) {
			delay[i].resize(CHANNELS, 0);
			attenuation[i].resize(CHANNELS, 1.f);
			avg[i].resize(CHANNELS);
			for (int j = 0; j < CHANNELS; j++) {
				avg[i][j].initialize(sample_rate, 4);
			}
		}

		inputBuffer.resize(CHANNELS, std::vector<float>(BUFFER_SIZE));
		delayBuffer.resize(CHANNELS, 0);
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
	} 

	void deactivate() {
	}

	float getInterpolatedValue(int ch, float age) {
		int index1, index2;
		float weight1, weight2;

		// Determine correct address in ring buffer
		age += generalBufferPointer;
		if (age < 0) age += BUFFER_SIZE;

		// Determine corresponding indices (not yet overflow corrected)
		index1 = age;
		index2 = index1 + 1;

		// Determine weight
		weight2 = age - (float) index1;
		weight1 = 1.0 - weight2;

		// Now correct overflow
		if (index2 == BUFFER_SIZE) index2 = 0;

		return inputBuffer[ch][index1] * weight1 + inputBuffer[ch][index2] * weight2;
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
			avg[0][i].pushData(delay[0][i], nframes);
			avg[1][i].pushData(delay[1][i], nframes);
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
		int batchsize, batches;
		float value;
		for (int i = 0; i < 2; i++) {
			batchsize = avg[i][0].getBatchSize();
			batches = nframes / batchsize;
			for (int b = 0; b < batches; b++) {
				// Fill delay buffer
				for (int ch = 0; ch < CHANNELS; ch++) delayBuffer[ch] = avg[i][ch].getData();
				for (int f = 0; f < batchsize; f++) {
					value = 0.f;
					// Get every frame from the right buffer
					for (int ch = 0; ch < CHANNELS; ch++) {
						value += inputBuffer[ch][(generalBufferPointer + f + b * batchsize) % BUFFER_SIZE];
						//value += (getInterpolatedValue(ch, (f + b * batchsize) - delayBuffer[ch]) * attenuation[i][ch]);
					}
					output[i][f + b * batchsize] = value;
				}
			}
		}
		generalBufferPointer += nframes;
		generalBufferPointer %= BUFFER_SIZE;
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
		double att = 1.0;

		for (int i = 0; i < COUNT; i++) {
			// Calculate position of source
			posx = r * sin(alpha_p[i]);
			posy = r * cos(alpha_p[i]);

			// Calculate distance to listener
			dist_l[i] = sqrt(pow((posx + eardist / 2.0), 2) + pow(posy, 2));
			dist_r[i] = sqrt(pow((posx - eardist / 2.0), 2) + pow(posy, 2));

			// Calculate attenuation and build product over attenuation
			attenuation[0][i] = r / dist_l[i];
			attenuation[0][i] = r / dist_r[i];
			att *= attenuation[0][i];
			att *= attenuation[1][i];

			// Calculate sample delay
			time_l = dist_l[i] / v_air;
			time_r = dist_r[i] / v_air;
			delay[0][i] = (int) round(time_l / (1.0 / sample_rate));
			delay[1][i] = (int) round(time_r / (1.0 / sample_rate));
		}

		// Normalize attenuation
		att = 1.f / att;
		for (int i = 0; i < COUNT; i++) {
			attenuation[0][i] *= att;
			attenuation[1][i] *= att;
		}
	}

private:
	int BUFFER_SIZE;

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

	std::vector<std::vector<int> > delay;
	std::vector<std::vector<float> > attenuation;
	std::vector<std::vector<SampleAverager> > avg;

	std::vector<std::vector<float> > inputBuffer;

	std::vector<float> delayBuffer;

	int generalBufferPointer;
};

static const lvtk::Descriptor<Pan> pan (PAN_URI);