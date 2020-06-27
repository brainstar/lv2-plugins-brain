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

	void resetPointers() {
		ptrFill = 0;
		ptrRead = 0;
	}

private:
	int iSize;
	int *vecData;
	long *vecSum;
	float *vecSumScaled;

	int ptrFill, ptrRead;
};