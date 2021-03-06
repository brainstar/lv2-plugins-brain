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

#include "pan.hpp"
#include <lvtk/plugin.hpp>

#define PAN_URI "http://github.com/brainstar/lv2/pan4"

class Pan4 : public Pan, public lvtk::Plugin<Pan4> {
public:
	Pan4(const lvtk::Args &args) : Plugin(args) {
		sample_rate = static_cast<float> (args.sample_rate);
		CHANNELS = 4;

		init((int) args.sample_rate);
	}

	~Pan4() { }

	void connect_port(uint32_t port, void* data) {
		connect_portBase(port, data);
	}

	void activate() {
		activateBase();
	}

	void deactivate() {
		deactivateBase();
	}

	void run(uint32_t nframes) {
		runBase(nframes);
	}
};


static const lvtk::Descriptor<Pan4> pan (PAN_URI);