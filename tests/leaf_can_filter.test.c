#include <stdio.h>
#include <string.h>
#include <math.h>

#include "leaf_can_filter.h"

bool cmp_floats_with_epsilon(float a, float b, float e)
{
	return fabs(a - b) <= e;
}

void bite_test_print_buf(uint8_t *buf)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		printf(BITE_YELLOW"0x%02X     ", buf[i]);
	}
	printf("\n");

	for (j = 0; j < 8; j++) {
		for (i = 8 - 1; i >= 0; i--) {
			printf((buf[j] & (1U << i)) ? "1" : "0");
		}
		printf(" ");
	}
	printf(BITE_CRST"\n\n");

	fflush(0);
}


void bec_test()
{
	float capacity = 7.0f;
	uint32_t update_interval_ms = 10U;
	uint32_t counts_per_hour = (1000U / update_interval_ms) * 60U * 60U;
	uint32_t ms_per_hour = 1000 * 60U * 60U;

	/* Calculate capacity counts */
	int64_t capacity_counts = (int64_t)(capacity * 1000.f) *
				            counts_per_hour;

	uint64_t i;
	
	struct bec bec;
	
	bec_init(&bec);
	bec_set_update_interval_ms (&bec, 10U);
	bec_set_full_cap_kwh       (&bec, capacity);
	bec_set_full_cap_voltage_V (&bec, 400.0f);
	bec_set_initial_cap_kwh    (&bec, 0.0f);

	assert(cmp_floats_with_epsilon(
		bec_get_remain_cap_kwh(&bec), 0.0, 0.001f));

	/* Put 350 volts and 5 amps during 1 hour */
	for (i = 0; i < ms_per_hour; i++) {
		bec_set_voltage_V(&bec, 350);
		bec_set_current_A(&bec, 5);
		bec_update(&bec, 1);
	}

	/* printf("bec._cap_counts: %lli\n", bec._cap_counts);
	 */
	printf("bec_get_remain_cap_kwh(&bec): %f\n",
		bec_get_remain_cap_kwh(&bec));
	assert(cmp_floats_with_epsilon(
		bec_get_remain_cap_kwh(&bec), 1.750, 0.1));

	printf("bec_get_soc_pct(&bec): %f\n", bec_get_soc_pct(&bec));
	assert(cmp_floats_with_epsilon(bec_get_soc_pct(&bec), 25.0, 0.1));

	/* Put 350 volts and 20 amps during 1 hour (minus one count) */
	bec_set_initial_cap_kwh(&bec, 0.0f);
	for (i = 0; i < ms_per_hour - update_interval_ms; i++) {
		bec_set_voltage_V(&bec, 350);
		bec_set_current_A(&bec, 20);
		bec_update(&bec, 1);
	}
	/* printf("bec._cap_counts: %lli\n", bec._cap_counts);
	 */
	/* Check edge case. Must be less than capacity */
	assert(bec._cap_counts < capacity_counts);
	printf("bec_get_remain_cap_kwh(&bec): %f\n",
		bec_get_remain_cap_kwh(&bec));

	/* Add one more count and check edge case 
	 * Must be equal to capacity */	
	bec_set_voltage_V(&bec, 350);
	bec_set_current_A(&bec, 20);
	bec_update(&bec, update_interval_ms);
	assert(bec._cap_counts == capacity_counts);
	printf("bec_get_remain_cap_kwh(&bec): %f\n",
		bec_get_remain_cap_kwh(&bec));

	/* Add one more count and check edge case 
	 * Should not exceed capacity */
	bec_set_voltage_V(&bec, 350);
	bec_set_current_A(&bec, 20);
	bec_update(&bec, update_interval_ms);
	assert(bec._cap_counts == capacity_counts);
	printf("bec_get_remain_cap_kwh(&bec): %f\n",
		bec_get_remain_cap_kwh(&bec));

	printf("bec_get_soc_pct(&bec): %f\n", bec_get_soc_pct(&bec));
	assert(cmp_floats_with_epsilon(bec_get_soc_pct(&bec), 100.0, 0.1));

	/* Put 350 volts and -20 amps during 1 hour (minus one count) */
	for (i = 0; i < ms_per_hour - update_interval_ms; i++) {
		bec_set_voltage_V(&bec, 350);
		bec_set_current_A(&bec, -20);
		bec_update(&bec, 1);
	}
	/* printf("bec._cap_counts: %lli\n", bec._cap_counts); */
	/* Check edge case. Must be more than 0.0 kwh */
	assert(bec._cap_counts > 0.0);
	printf("bec_get_remain_cap_kwh(&bec): %f\n",
		bec_get_remain_cap_kwh(&bec));

	/* Add one more count and check edge case 
	 * Must be equal to 0.0kwh */	
	bec_set_voltage_V(&bec, 350);
	bec_set_current_A(&bec, -20);
	bec_update(&bec, update_interval_ms);
	assert(bec._cap_counts == 0.0);
	printf("bec_get_remain_cap_kwh(&bec): %f\n",
		bec_get_remain_cap_kwh(&bec));

	/* Add one more count and check edge case 
	 * Should not go below 0.0kwh */
	bec_set_voltage_V(&bec, 350);
	bec_set_current_A(&bec, -20);
	bec_update(&bec, update_interval_ms);
	assert(bec._cap_counts == 0.0);
	printf("bec_get_remain_cap_kwh(&bec): %f\n",
		bec_get_remain_cap_kwh(&bec));

	printf("bec_get_soc_pct(&bec): %f\n", bec_get_soc_pct(&bec));
	assert(cmp_floats_with_epsilon(bec_get_soc_pct(&bec), 0.0, 0.1));

	/* Check if 100% voltage sets energy too capacity */
	bec_set_voltage_V(&bec, 400);
	bec_set_current_A(&bec, 1);
	bec_update(&bec, update_interval_ms);
	/* LEGACY: assert(bec._cap_counts == capacity_counts);
	 * Now we have to wait at least 5 seconds for get to 100% */
	for (i = 0; i < 4999 - update_interval_ms; i++) {
		bec_update(&bec, 1);
	}
	assert(bec._cap_counts != capacity_counts);
	bec_update(&bec, 1);
	assert(bec._cap_counts == capacity_counts);

	printf("bec_get_soc_pct(&bec): %f\n", bec_get_soc_pct(&bec));
	assert(cmp_floats_with_epsilon(bec_get_soc_pct(&bec), 100.0, 0.1));
}

void leaf_can_filter_test_1468U(struct leaf_can_filter *self, struct bite *bi)
{
	uint16_t capacity_gids = 0U;

	struct leaf_can_filter_frame frame;
	
	const uint8_t test_data[8] = {
		0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00};

	frame.id = 1468U;
	frame.len = 8U;
	memcpy(frame.data, test_data, frame.len);
	leaf_can_filter_process_frame(self, &frame);
	leaf_can_filter_update(self, 0);
	
	bite_set_buf(bi, frame.data, frame.len);

	/* SG_ LB_Remain_Capacity :
	 * 	7|10@0+ (1,0) [0|500] "gids" Vector__XXX */
	bite_begin(bi, 7U, 10U, BITE_ORDER_DBC_0);
	capacity_gids = bite_read_u16(bi);
	bite_end(bi);

	/* Real capacity reported by BMS */
	printf("remain_capacity_wh: %u\n", self->_bms_vars.remain_capacity_wh);

	/* Filtered capacity */
	printf("remaining_capacity_filtered_wh: %u\n", capacity_gids * 80U);

	assert(capacity_gids == 63U); /* Manual capacity was set */

	/* set full capacity mux now*/
	memcpy(frame.data, test_data, frame.len);
	frame.data[5] = 0x10;
	leaf_can_filter_process_frame(self, &frame);
	leaf_can_filter_update(self, 0);
	
	bite_set_buf(bi, frame.data, frame.len);

	/* SG_ LB_Remain_Capacity :
	 * 	7|10@0+ (1,0) [0|500] "gids" Vector__XXX */
	bite_begin(bi, 7U, 10U, BITE_ORDER_DBC_0);
	capacity_gids = bite_read_u16(bi);
	bite_end(bi);

	printf("full_capacity_wh: %u\n", self->_bms_vars.full_capacity_wh);

	/* Filtered capacity */
	printf("full_capacity_filtered_wh: %u\n", capacity_gids * 80U);

	assert(capacity_gids == 300U);

	bite_test_print_buf(frame.data);
}

void leaf_can_filter_test_475U(struct leaf_can_filter *self, struct bite *bi)
{
	uint32_t i;
	uint32_t ms_per_min = 1000 * 60U;

	uint16_t LB_Total_Voltage = 0;
	int16_t  LB_Current       = 0;

	struct leaf_can_filter_frame frame;

	/*const uint8_t test_data[8] = {
		0x2B, 0xC0, 0xD4, 0x80, 0x00, 0x00, 0x00, 0x00};*/
	
	const uint8_t test_data[8] = {
		0xA8, 0x80, 0xD4, 0x80, 0x00, 0x00, 0x00, 0x00};

	frame.id = 475U;
	frame.len = 8U;
	memcpy(frame.data, test_data, frame.len);
	leaf_can_filter_process_frame(self, &frame);
	leaf_can_filter_update(self, 0);
	
	bite_set_buf(bi, frame.data, frame.len);

	/* SG_ LB_Total_Voltage : 
	 * 	23|10@0+ (0.5,0) [0|450] "V" Vector__XXX */
	bite_begin(bi, 23, 10, BITE_ORDER_DBC_0);
	LB_Total_Voltage |= ((uint16_t)bite_read(bi)) << 2;
	LB_Total_Voltage |= ((uint16_t)bite_read(bi)) << 0;
	bite_end(bi);

	/* SG_ LB_Current :
	 * 	7|11@0+ (0.5,0) [-400|200] "A" Vector__XXX */
	bite_begin(bi, 7, 11, BITE_ORDER_DBC_0);
	LB_Current = bite_read_i16(bi);
	bite_end(bi);

	assert(LB_Total_Voltage == (425 * 2));
	/* assert(LB_Current == (175 * 2)); */

	assert(LB_Current == (-350 * 2));

	printf("_voltage_V: %i\n", self->_bec._voltage_V);
	printf("_current_A: %i\n", self->_bec._current_A);
	assert(self->_bec._voltage_V == 425);
	assert(self->_bec._current_A == -350);

	/* Keep discharge for 2 mins, this should drop about ~5kwh 
	 * (It's ok since current and voltage are huge) */
	for (i = 0; i < ms_per_min * 2; i++) {
		leaf_can_filter_update(self, 1);
	}
}

void leaf_can_filter_test()
{
	struct leaf_can_filter fi;
	/* struct leaf_can_filter_frame frame; */
	struct bite bi;

	/* const uint8_t test_data_0[8] = {0}; */

	leaf_can_filter_init(&fi);
	fi.settings.bypass = false; /* disable bypass */
	fi.settings.capacity_override_enabled = true;
	
	bec_set_full_cap_kwh(&fi._bec, 24.0f);
	bec_set_full_cap_voltage_V(&fi._bec, 500.0f);
	bec_set_initial_cap_kwh(&fi._bec, 10.0f);

	/* test segments
	frame.id = 1468U;
	frame.len = 8U;
	memcpy(frame.data, test_data_0, frame.len);
	leaf_can_filter_process_frame(&fi, &frame);
	leaf_can_filter_update(&fi, 0);

	bite_set_buf(&bi, frame.data, frame.len);
	bite_begin(&bi, 16, 4, BITE_ORDER_DBC_1);
	assert(bite_read(&bi) == 12);
	bite_end(&bi); */
	
	bite_init(&bi);
	leaf_can_filter_test_475U(&fi, &bi);
	leaf_can_filter_test_1468U(&fi, &bi);
}

void led_indicator_test()
{
	struct dev_timeout_led_indicator li;

	/* Test invalid config */
	dev_timeout_led_indicator_init(&li);
	assert(dev_timeout_led_indicator_update(&li, 0) == false);
	assert(li._state == DEV_TIMEOUT_LED_INDICATOR_STATE_CONFIG_INVALID);

	/* Test timeout */
	dev_timeout_led_indicator_init(&li);
	dev_timeout_led_indicator_set_count(&li, 2);
	assert(dev_timeout_led_indicator_update(&li, 0) == true);
	/* printf("%i\n", li._state); */
	assert(li._state == DEV_TIMEOUT_LED_INDICATOR_STATE_WAIT);

	assert(dev_timeout_led_indicator_update(&li, 750) == false);
	assert(li._state == DEV_TIMEOUT_LED_INDICATOR_STATE_BLINK_0);

	assert(dev_timeout_led_indicator_update(&li, 200) == true);
	assert(li._state == DEV_TIMEOUT_LED_INDICATOR_STATE_BLINK_1);

	assert(dev_timeout_led_indicator_update(&li, 50) == false);
	assert(li._state == DEV_TIMEOUT_LED_INDICATOR_STATE_TIMEOUT);
	assert(li._dev_sel == 1);

	/* Two blinks */
	assert(dev_timeout_led_indicator_update(&li, 0) == true);
	assert(li._state == DEV_TIMEOUT_LED_INDICATOR_STATE_WAIT);
	assert(dev_timeout_led_indicator_update(&li, 750) == false);
	assert(dev_timeout_led_indicator_update(&li, 200) == true);
	assert(dev_timeout_led_indicator_update(&li, 50) == true);
	assert(li._dev_sel == 1);

	assert(dev_timeout_led_indicator_update(&li, 0) == false);
	assert(li._state == DEV_TIMEOUT_LED_INDICATOR_STATE_BLINK_0);
	assert(dev_timeout_led_indicator_update(&li, 200) == true);
	assert(dev_timeout_led_indicator_update(&li, 50) == false);
	assert(li._dev_sel == 1);

	dev_timeout_led_indicator_update_timer(&li, 0, 500);
	dev_timeout_led_indicator_update_timer(&li, 1, 500);
	assert(li._state == DEV_TIMEOUT_LED_INDICATOR_STATE_NORMAL);
	assert(dev_timeout_led_indicator_update(&li, 0) == true);

	assert(dev_timeout_led_indicator_update(&li, 0) == false);

	assert(dev_timeout_led_indicator_update(&li, 1000) == false);
	assert(dev_timeout_led_indicator_update(&li, 0) == true);
	assert(li._state == DEV_TIMEOUT_LED_INDICATOR_STATE_WAIT);
}

int main()
{
	leaf_can_filter_test();
	bec_test();
	led_indicator_test();
	return 0;
}
