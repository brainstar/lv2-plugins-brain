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

class TriangularAverage
{
public:
	TriangularAverage() {
		vecData = nullptr;
		vecSum = nullptr;
		vecSumScaled = nullptr;
	}
	~TriangularAverage() {
		delete[] vecData;
		delete[] vecSum;
		delete[] vecSumScaled;
	}

	void init(int size, int windowSize = 0) {
		iSize = size;

		vecData = new int[iSize];
		vecSum = new long[iSize];
		vecSumScaled = new float[iSize];

		// Calculate distances using a window size just a little bit smaller than the buffer
		// TODO: Maybe increase buffer size for fewer overruns and better overrun detection?
		if (windowSize) setWindowSize(windowSize);
        else resetWindowSize();

		clean();
	}

    void resetWindowSize() {
        setWindowSize(iSize * 0.95);
    }

	int getWindowSize() {
		return iWindowSize;
	}

    void setWindowSize(int size) {
        if (size >= iSize) size = iSize - 1;
        if (size % 2 != 0) size--;
		iWindowSize = size;
        
		vecOffset[0] = -(size + 1);
		vecOffset[1] = -(size / 2 + 1);
		vecOffset[2] = vecOffset[1] + 1;

        size /= 2;
        fScalingFactor = (1.f + size) * size;

		clean();
    }

	void clean() {
		for (int i = 0; i < iSize; i++) {
			vecData[i] = 0;
			vecSum[i] = 0;
			vecSumScaled[i] = 0.0;
		}

		iStep = 0;

		ptrFill = 0;
		ptrRead = 0;
	}

    void reduce(int &position) {
        while (position < 0) position += iSize;
        while (position >= iSize) position -= iSize;
    }

    int getRelPos(int position = 0) {
        return getAbsPos(position + ptrFill);
    }

    int getAbsPos(int position) {
        reduce(position);
        return position;
    }

	// Push data points into the average filter
	void pushData(int value, int length = 1) {
		if (length < 1) return;

        int target = ptrFill + length;
        for (; ptrFill < target; ptrFill++) {
            iStep += (value
                + vecData[getRelPos(vecOffset[0])]
                - vecData[getRelPos(vecOffset[1])]
                - vecData[getRelPos(vecOffset[2])]);
            vecData[getRelPos()] = value;
            // TODO
            vecSum[getRelPos()] = vecSum[getRelPos(-1)] + iStep;
            vecSumScaled[getRelPos()] = vecSum[getRelPos()] / fScalingFactor;
        }

        ptrFill %= iSize;
	}

	// Get data point out of the filter and increment ptrRead
	float popData() {
		if (ptrRead == iSize) ptrRead = 0;
		return vecSumScaled[ptrRead++];
	}

	// Read out data without incrementing ptrRead
	float readData(int offset = 0) {
		int position = ptrRead + offset;
        reduce(position);

		return vecSumScaled[position];
	}

	void resetPointers() {
		ptrFill = 0;
		ptrRead = 0;
	}

private:
	int iSize, iWindowSize;
	int iStep;
	float fScalingFactor;
	int *vecData;
	long *vecSum;
	float *vecSumScaled;
	int vecOffset[3];

	int ptrFill, ptrRead;
};