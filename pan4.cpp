#include <lvtk/plugin.hpp>
using namespace lvtk;

class Pan : public Plugin<Pan> {
public:
	Pan(double rate) : Plugin<Pan>(8) {
	}

	void run(uint32_t nframes) {
		for (uint32_t i = 0; i < nframes; ++i) {
			p(7)[i] = 0;
			p(8)[i] = 0;
		}
	}
};

static int _ = Pan::register_class("http://github.com/brainstar/lv2/pan4");