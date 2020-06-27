//To enable DAZ
#include <pmmintrin.h>
//To enable FTZ
#include <xmmintrin.h>

#include <math.h>
#include <vector>
#include <lvtk/plugin.hpp>

#define PAN_URI "http://github.com/brainstar/lv2/pan4"
const int CHANNELS = 4;

class MovingAverage
{
public:
	MovingAverage() {
		vecData = nullptr;
		vecSum = nullptr;
		vecSumScaled = nullptr;
	}
	~MovingAverage() {
		delete[] vecData;
		delete[] vecSum;
		delete[] vecSumScaled;
	}

	void init(int size) {
		iSize = size;

		vecData = new int[iSize];
		vecSum = new long[iSize];
		vecSumScaled = new float[iSize];
		// vecData = std::vector<int>(iSize, 0);
		// vecSum = std::vector<long>(iSize, 0);
		// vecSumScaled = std::vector<float>(iSize, 0);

		clean();
	}

	void clean() {
		for (int i = 0; i < iSize; i++) {
			vecData[i] = 0;
			vecSum[i] = 0;
			vecSumScaled[i] = 0.0;
		}

		ptrFill = 0;
		ptrRead = 0;
	}

	// Push data points into the average filter
	void pushData(int value, int length = 1) {
		// Sanity check
		if (length < 1) return;

		// If you're expecting lengths > iSize, youshould write a small
		// shortcut here to reset all vectors to the designated values
		// As this is an RT application, there is no reason to
		// anticipate this kind of behaviour and this comparison would
		// slow the programm down a little bit
		/*if (length >= iSize) {
			long sum = value * iSize;
			for (int i = 0; i < iSize; i++) {
				vecData[i] = value;
				vecSum[i] = sum;
				vecSumScaled[i] = value;
			}
			ptrFill += length;
			ptrFill %= iSize;
		}*/

		// Prevent buffer underrun
		if (ptrFill == 0) {
			vecSum[0] = vecSum[iSize - 1] - vecData[0] + value;
			vecData[0] = value;
			vecSumScaled[0] = (float) vecSum[0] / (float) iSize;
			length--;
			ptrFill++;
		}

		// No buffer overrun?
		int target = ptrFill + length;
		if (target <= iSize) {
			// Just copy all values
			for (; ptrFill < target; ptrFill++) {
				vecSum[ptrFill] = vecSum[ptrFill - 1] - vecData[ptrFill] + value;
				vecData[ptrFill] = value;
				vecSumScaled[ptrFill] = (float) vecSum[ptrFill] / (float) iSize;
			}
			// If necessary reset ptrFill
			if (ptrFill == iSize) ptrFill = 0;
		} else {
			// Rescale target to [0, iSize[
			target -= iSize;

			// Fill in from [ptrFill, iSize[
			for (; ptrFill < iSize; ptrFill++) {
				vecSum[ptrFill] = vecSum[ptrFill - 1] - vecData[ptrFill] + value;
				vecData[ptrFill] = value;
				vecSumScaled[ptrFill] = (float) vecSum[ptrFill] / (float) iSize;
			}
			// Fill in the 0 (buffer underrun prevention)
			// 0 is filled, because of "if (target <= iSize)"
			vecSum[0] = vecSum[iSize - 1] - vecData[0] + value;
			vecData[0] = value;
			vecSumScaled[0] = (float) vecSum[0] / (float) iSize;

			// Fill in the rest
			for (ptrFill = 1; ptrFill < target; ptrFill++) {
				vecSum[ptrFill] = vecSum[ptrFill - 1] - vecData[ptrFill] + value;
				vecData[ptrFill] = value;
				vecSumScaled[ptrFill] = (float) vecSum[ptrFill] / (float) iSize;
			}
		}
	}

	// Get data point out of the filter and increment ptrRead
	float popData() {
		if (ptrRead == iSize) ptrRead = 0;
		return vecSumScaled[ptrRead++];
	}

	// Read out data without incrementing ptrRead
	float readData(int offset = 0) {
		int position = ptrRead + offset;
		while (position >= iSize) position -= iSize;
		while (position < 0) position += iSize;

		return vecSumScaled[position];
	}

private:
	int iSize;
	int *vecData;
	long *vecSum;
	float *vecSumScaled;
	// std::vector<int> vecData;
	// std::vector<long> vecSum;
	// std::vector<float> vecSumScaled;

	int ptrFill, ptrRead;
};
/*
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
};*/

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

		avgBatchSize = 8;
		int batches;
		while ((int) args.sample_rate % avgBatchSize != 0) avgBatchSize /= 2;
		batches = (int) args.sample_rate / avgBatchSize;

		delay = new int*[2];
		attenuation = new float*[2];
		avg = new MovingAverage*[2];

		for (int i = 0; i < 2; i++) {
			avg[i] = new MovingAverage[CHANNELS];
			attenuation[i] = new float[CHANNELS];
			delay[i] = new int[CHANNELS];

			for (int j = 0; j < CHANNELS; j++) {
				avg[i][j].init(batches);
				attenuation[i][j] = 1.f;
				delay[i][j] = 0;
			}
		}

		inputBuffer = new float*[CHANNELS];
		delayBuffer = new float[CHANNELS];
		for (int ch = 0; ch < CHANNELS; ch++) {
			delayBuffer[ch] = 0.f;
			inputBuffer[ch] = new float[BUFFER_SIZE];
			for (int i = 0; i < BUFFER_SIZE; i++) {
				inputBuffer[ch][i] = 0.f;
			}
		}
		
		generalBufferPointer = 0;
	}

	~Pan() {
		for (int i = 0; i < 2; i++) {
			delete[] avg[i];
			delete[] attenuation[i];
			delete[] delay[i];
		}

		for (int ch = 0; ch < CHANNELS; ch++) {
			delete[] inputBuffer[ch];
		}
		
		delete[] delayBuffer;
		delete[] avg;
		delete[] attenuation;
		delete[] delay;
		delete[] inputBuffer;
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

		// TODO: What if nframes % batchsize != 0??? (Should not be the case, but Murphy)
		for (int i = 0; i < CHANNELS; i++) {
			avg[0][i].pushData(delay[0][i], nframes / avgBatchSize);
			avg[1][i].pushData(delay[1][i], nframes / avgBatchSize);
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
		for (int i = 0; i < 2; i++) {
			batches = nframes / avgBatchSize;
			for (int b = 0; b < batches; b++) {
				// Fill delay buffer
				// for (int ch = 0; ch < CHANNELS; ch++) delayBuffer[ch] = avg[i][ch].popData();
				for (int f = 0; f < avgBatchSize; f++) {
					value = 0.f;
					// Get every frame from the right buffer
					for (int ch = 0; ch < CHANNELS; ch++) {
						value += (getInterpolatedValue(ch, (f + b * avgBatchSize)) * attenuation[i][ch]);
						// value += (getInterpolatedValue(ch, (f + b * avgBatchSize) - delayBuffer[ch]) * attenuation[i][ch]);
					}
					output[i][f + b * avgBatchSize] = value;
				}
			}
		}
		generalBufferPointer += nframes;
		if (generalBufferPointer >= BUFFER_SIZE) generalBufferPointer -= BUFFER_SIZE;
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

	int** delay;
	float** attenuation;
	MovingAverage** avg;

	float** inputBuffer;

	float *delayBuffer;

	int generalBufferPointer;

	int avgBatchSize;
};

static const lvtk::Descriptor<Pan> pan (PAN_URI);