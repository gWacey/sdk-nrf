#include <zephyr/kernel.h>
#include <dsp/filtering_functions.h>

#include <zephyr/timing/timing.h>

#include "nrfx_clock.h"
#include "white20ms_16b16khz_mono.h"

#define FILTER_TAP_NUM 180
#define BLOCK_SIZE     WHITE20MS_16B16KHZ_MONO_SIZE
#define STATE_SIZE     (FILTER_TAP_NUM / 3) + BLOCK_SIZE - 1

q15_t filter_taps[FILTER_TAP_NUM] = {
	93,    142,   154,   79,   -62,	 -199,	-250,  -192, -76,  8,	  3,	 -68,	-127, -111,
	-30,   41,    39,    -31,  -97,	 -89,	-10,   66,   65,   -14,	  -94,	 -92,	-4,   86,
	88,    -4,    -102,  -106, -6,	 104,	113,   6,    -116, -129,  -13,	 123,	142,  17,
	-134,  -160,  -25,   144,  177,	 32,	-156,  -199, -42,  169,	  223,	 53,	-184, -251,
	-66,   201,   284,   82,   -221, -323,	-102,  244,  370,  126,	  -272,	 -429,	-157, 308,
	504,   197,   -355,  -606, -253, 420,	751,   336,  -516, -977,  -469,	 679,	1379, 725,
	-1016, -2311, -1414, 2157, 6951, 10357, 10357, 6951, 2157, -1414, -2311, -1016, 725,  1379,
	679,   -469,  -977,  -516, 336,	 751,	420,   -253, -606, -355,  197,	 504,	308,  -157,
	-429,  -272,  126,   370,  244,	 -102,	-323,  -221, 82,   284,	  201,	 -66,	-251, -184,
	53,    223,   169,   -42,  -199, -156,	32,    177,  144,  -25,	  -160,	 -134,	17,   142,
	123,   -13,   -129,  -116, 6,	 113,	104,   -6,   -106, -102,  -4,	 88,	86,   -4,
	-92,   -94,   -14,   65,   66,	 -10,	-89,   -97,  -31,  39,	  41,	 -30,	-111, -127,
	-68,   3,     8,     -76,  -192, -250,	-199,  -62,  79,   154,	  142,	 93};

q15_t destination_buf[BLOCK_SIZE * 3];

arm_fir_interpolate_instance_q15 fir;

q15_t state_buf[STATE_SIZE];
static uint8_t L = 48000 / 16000;

static int hfclock_config_and_start(void)
{
	int ret;

	/* Use this to turn on 128 MHz clock for cpu_app */
	ret = nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);

	ret -= NRFX_ERROR_BASE_NUM;
	if (ret) {
		return ret;
	}

	nrfx_clock_hfclk_start();
	while (!nrfx_clock_hfclk_is_running()) {
	}

	return 0;
}

int main(void)
{
	printk("Halla verden\n");

	arm_status err;

	timing_t start_time, end_time;
	uint64_t total_cycles;
	uint64_t total_ns;

	hfclock_config_and_start();
	timing_init();
	timing_start();

	/* Initialize FIR instance */
	err = arm_fir_interpolate_init_q15(&fir, L, FILTER_TAP_NUM, filter_taps, state_buf,
					   BLOCK_SIZE);
	if (err != ARM_MATH_SUCCESS) {
		printk("arm_fir_decimate_init_q15: %d\n", err);
		while (1) {
		};
	}

	start_time = timing_counter_get();

	arm_fir_interpolate_q15(&fir, test_data_in, destination_buf, BLOCK_SIZE);

	end_time = timing_counter_get();
	total_cycles = timing_cycles_get(&start_time, &end_time);
	total_ns = timing_cycles_to_ns(total_cycles);
	timing_stop();

	/*for (int i = 0; i < BLOCK_SIZE; i++) {
		printk("%d: %d\n", i, destination_buf[i]);
	}*/

	printk("Time used: %lf ms\n", total_ns / 1000000.0);

	printk("Finished decimating\n");
}
