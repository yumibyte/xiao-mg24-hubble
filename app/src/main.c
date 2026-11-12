/*
 * Copyright (c) 2026 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/hubble.h>
#include <hubble/sat/packet.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/base64.h>
#include <zephyr/sys/byteorder.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(main);

/*
 * Pulsed after each packet sent, so a board running with logging off (see
 * prj.conf) still shows it is transmitting. Boards without a led0 alias get an
 * empty spec and skip the pulse.
 */
#define LED_PULSE_MS 50
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

/*
 * Every packet carries the largest payload the satellite packet allows (the SDK
 * only accepts 0, 4, 9 or 13 bytes). Until real data goes here: bytes 0-3 are a
 * big-endian sequence number, so received packets can be counted and gaps
 * spotted, and bytes 4-12 hold their own index (0x04..0x0c), so a correct
 * decode is easy to recognise.
 */
static uint8_t payload[HUBBLE_SAT_PAYLOAD_MAX];

/* Every packet in the window gets a new sequence number; see Kconfig. */
BUILD_ASSERT(CONFIG_SAMPLE_SAT_TX_WINDOW_SECONDS == 0 ||
		     DIV_ROUND_UP(CONFIG_SAMPLE_SAT_TX_WINDOW_SECONDS,
				  CONFIG_SAMPLE_SAT_TX_INTERVAL_SECONDS) <= 1024,
	     "Transmit window needs more than the 1,024 sequence numbers per day");

/*
 * Decoded master key. The SDK stores this pointer directly, so the buffer has
 * static storage duration and outlives all SDK calls. Zero-initialised, which
 * also serves as the all-zero placeholder when no key is provided.
 */
static uint8_t master_key[CONFIG_HUBBLE_KEY_SIZE];

/*
 * Decode the base64 master key (CONFIG_SAMPLE_HUBBLE_KEY, provisioned at build
 * time) into master_key, validating its length.
 */
static int master_key_provision(void)
{
	const char *key = CONFIG_SAMPLE_HUBBLE_KEY;
	size_t olen;
	int err;

	if (strlen(key) == 0) {
		LOG_WRN("CONFIG_SAMPLE_HUBBLE_KEY not set; using an all-zero "
			"placeholder key. Pass your base64 key with "
			"-DCONFIG_SAMPLE_HUBBLE_KEY=\"<key>\".");
		return 0;
	}

	err = base64_decode(master_key, sizeof(master_key), &olen, key,
			    strlen(key));
	if (err != 0 || olen != sizeof(master_key)) {
		LOG_ERR("Invalid CONFIG_SAMPLE_HUBBLE_KEY: expected a base64-"
			"encoded %u-byte key (matching CONFIG_HUBBLE_KEY_SIZE)",
			(unsigned int)sizeof(master_key));
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_SAMPLE_PROVIDE_SAT_BOARD_SUPPORT

int hubble_sat_board_init(void)
{
	return 0;
}

int hubble_sat_board_enable(void)
{
	return 0;
}

int hubble_sat_board_disable(void)
{
	return 0;
}

int hubble_sat_board_packet_send(const struct hubble_sat_packet_frames *packet)
{
	ARG_UNUSED(packet);

	return 0;
}

#endif /* CONFIG_SAMPLE_PROVIDE_SAT_BOARD_SUPPORT */

#ifdef CONFIG_SAMPLE_SAT_TX_TIMING
#include <zephyr/timing/timing.h>

/*
 * Filled by the wrapper below for the most recent radio send. Two clocks:
 * the DWT CPU cycle counter stops while the core sleeps (the port waits on a
 * semaphore for each symbol), so it gives CPU-active time; the system clock
 * (k_cycle_get_32, the 32.768 kHz RTC on nRF) keeps running and gives wall-clock time.
 */
static uint64_t radio_cycles;
static uint32_t radio_sys_cycles;
static size_t radio_symbols;

int __real_hubble_sat_board_packet_send(const struct hubble_sat_packet_frames *packet);

/*
 * The linker routes the SDK's call to the board radio send here (see
 * CMakeLists.txt), so this times only putting the symbols on the air:
 * no packet encoding, radio enable/disable or FEM power-up in the port.
 */
int __wrap_hubble_sat_board_packet_send(const struct hubble_sat_packet_frames *packet)
{
	uint32_t s0 = k_cycle_get_32();
	timing_t t0 = timing_counter_get();
	int ret = __real_hubble_sat_board_packet_send(packet);
	timing_t t1 = timing_counter_get();
	uint32_t s1 = k_cycle_get_32();

	radio_cycles = timing_cycles_get(&t0, &t1);
	radio_sys_cycles = s1 - s0;
	radio_symbols = packet->total_number_of_symbols;

	return ret;
}

static void tx_timing_print(uint32_t seq, uint32_t send_sys_cycles)
{
	uint64_t wall_us = k_cyc_to_us_near64(radio_sys_cycles);
	uint64_t cpu_us = timing_cycles_to_ns(radio_cycles) / 1000U;
	uint64_t send_us = k_cyc_to_us_near64(send_sys_cycles);
	uint64_t per_symbol_us = radio_symbols ? wall_us / radio_symbols : 0;

	printk("tx seq %u: %u symbols, radio wall %llu us (%llu us/symbol), "
	       "radio CPU-active %llu us, hubble_sat_packet_send wall %llu us\n",
	       seq, (unsigned int)radio_symbols, wall_us, per_symbol_us, cpu_us, send_us);
}
#endif /* CONFIG_SAMPLE_SAT_TX_TIMING */


int main(void)
{
	int err = 0;
	struct hubble_sat_packet pkt;

	LOG_DBG("Hubble Network Sat Sample started");

	err = master_key_provision();
	if (err != 0) {
		goto end;
	}

	/*
	 * This sample uses the device uptime counter source
	 * (CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME), so no UTC time is needed:
	 * we pass 0 as the initial EID counter value and it advances with uptime.
	 */
	err = hubble_init(0, master_key);
	if (err != 0) {
		LOG_ERR("Failed to initialize Hubble Sat Network");
		goto end;
	}

#ifdef CONFIG_SAMPLE_SAT_TX_TIMING
	timing_init();
	timing_start();
#endif

	bool led_ok = gpio_is_ready_dt(&led) &&
		      gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE) == 0;

	for (size_t i = sizeof(uint32_t); i < sizeof(payload); i++) {
		payload[i] = i;
	}

	k_sleep(K_SECONDS(CONFIG_SAMPLE_SAT_TX_START_DELAY_SECONDS));
	int64_t start_ms = k_uptime_get();

	for (uint32_t seq = 0;; seq++) {
		/* Scheduled from start_ms so time on air doesn't stretch the period. */
		int64_t at_ms = start_ms + (int64_t)seq *
			CONFIG_SAMPLE_SAT_TX_INTERVAL_SECONDS * MSEC_PER_SEC;

		if (CONFIG_SAMPLE_SAT_TX_WINDOW_SECONDS > 0 &&
		    at_ms - start_ms >= CONFIG_SAMPLE_SAT_TX_WINDOW_SECONDS * MSEC_PER_SEC) {
			break;
		}
		k_sleep(K_TIMEOUT_ABS_MS(at_ms));

		sys_put_be32(seq, payload);

		err = hubble_sat_packet_get(&pkt, payload, sizeof(payload));
		if (err != 0) {
			LOG_ERR("Failed to get Hubble Sat Network packet");
			goto end;
		}

		/* Set reliability to NONE - this will trigger a single
		 * transmission instead of a sequence. This is only for testing
		 * purposes and not recommended for production.
		 */
#ifdef CONFIG_SAMPLE_SAT_TX_TIMING
		uint32_t send_t0 = k_cycle_get_32();
#endif
		err = hubble_sat_packet_send(&pkt, HUBBLE_SAT_RELIABILITY_NONE);
#ifdef CONFIG_SAMPLE_SAT_TX_TIMING
		uint32_t send_t1 = k_cycle_get_32();
#endif
		if (err != 0) {
			LOG_ERR("Failed to transmit packet");
			goto end;
		}
#ifdef CONFIG_SAMPLE_SAT_TX_TIMING
		tx_timing_print(seq, (uint32_t)(send_t1 - send_t0));
#endif

		/* After the send returns, so the GPIO never toggles mid-packet. */
		if (led_ok) {
			gpio_pin_set_dt(&led, 1);
			k_sleep(K_MSEC(LED_PULSE_MS));
			gpio_pin_set_dt(&led, 0);
		}
	}

end:
	return err;
}
