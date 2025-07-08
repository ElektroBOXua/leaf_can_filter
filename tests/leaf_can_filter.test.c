#include <stdio.h>
#include <math.h>
#include "leaf_can_filter.h"

bool cmp_floats_with_epsilon(float a, float b, float e)
{
	return fabs(a - b) <= e;
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
	bec_set_update_interval_ms  (&bec, 10U);
	bec_set_battery_capacity_kwh(&bec, capacity);
	bec_set_initial_energy_kwh  (&bec, 0.0f);
	bec_set_100_pct_voltage_V   (&bec, 400.0f);

	assert(cmp_floats_with_epsilon(bec_get_energy_kwh(&bec), 0.0, 0.001f));

	/* Put 350 volts and 5 amps during 1 hour */
	for (i = 0; i < ms_per_hour; i++) {
		bec_set_voltage_V(&bec, 350);
		bec_set_current_A(&bec, 5);
		bec_update(&bec, 1);
	}

	/* printf("bec._energy_counts: %lli\n", bec._energy_counts);
	 */
	printf("bec_get_energy_kwh(&bec): %f\n", bec_get_energy_kwh(&bec));
	assert(cmp_floats_with_epsilon(bec_get_energy_kwh(&bec), 1.750, 0.1));

	printf("bec_get_soc_pct(&bec): %f\n", bec_get_soc_pct(&bec));
	assert(cmp_floats_with_epsilon(bec_get_soc_pct(&bec), 25.0, 0.1));

	/* Put 350 volts and 20 amps during 1 hour (minus one count) */
	bec_set_initial_energy_kwh(&bec, 0.0f);
	for (i = 0; i < ms_per_hour - update_interval_ms; i++) {
		bec_set_voltage_V(&bec, 350);
		bec_set_current_A(&bec, 20);
		bec_update(&bec, 1);
	}
	/* printf("bec._energy_counts: %lli\n", bec._energy_counts);
	 */
	/* Check edge case. Must be less than capacity */
	assert(bec._energy_counts < capacity_counts);
	printf("bec_get_energy_kwh(&bec): %f\n", bec_get_energy_kwh(&bec));

	/* Add one more count and check edge case 
	 * Must be equal to capacity */	
	bec_set_voltage_V(&bec, 350);
	bec_set_current_A(&bec, 20);
	bec_update(&bec, update_interval_ms);
	assert(bec._energy_counts == capacity_counts);
	printf("bec_get_energy_kwh(&bec): %f\n", bec_get_energy_kwh(&bec));

	/* Add one more count and check edge case 
	 * Should not exceed capacity */
	bec_set_voltage_V(&bec, 350);
	bec_set_current_A(&bec, 20);
	bec_update(&bec, update_interval_ms);
	assert(bec._energy_counts == capacity_counts);
	printf("bec_get_energy_kwh(&bec): %f\n", bec_get_energy_kwh(&bec));

	printf("bec_get_soc_pct(&bec): %f\n", bec_get_soc_pct(&bec));
	assert(cmp_floats_with_epsilon(bec_get_soc_pct(&bec), 100.0, 0.1));

	/* Put 350 volts and -20 amps during 1 hour (minus one count) */
	for (i = 0; i < ms_per_hour - update_interval_ms; i++) {
		bec_set_voltage_V(&bec, 350);
		bec_set_current_A(&bec, -20);
		bec_update(&bec, 1);
	}
	/* printf("bec._energy_counts: %lli\n", bec._energy_counts); */
	/* Check edge case. Must be more than 0.0 kwh */
	assert(bec._energy_counts > 0.0);
	printf("bec_get_energy_kwh(&bec): %f\n", bec_get_energy_kwh(&bec));

	/* Add one more count and check edge case 
	 * Must be equal to 0.0kwh */	
	bec_set_voltage_V(&bec, 350);
	bec_set_current_A(&bec, -20);
	bec_update(&bec, update_interval_ms);
	assert(bec._energy_counts == 0.0);
	printf("bec_get_energy_kwh(&bec): %f\n", bec_get_energy_kwh(&bec));

	/* Add one more count and check edge case 
	 * Should not go below 0.0kwh */
	bec_set_voltage_V(&bec, 350);
	bec_set_current_A(&bec, -20);
	bec_update(&bec, update_interval_ms);
	assert(bec._energy_counts == 0.0);
	printf("bec_get_energy_kwh(&bec): %f\n", bec_get_energy_kwh(&bec));

	printf("bec_get_soc_pct(&bec): %f\n", bec_get_soc_pct(&bec));
	assert(cmp_floats_with_epsilon(bec_get_soc_pct(&bec), 0.0, 0.1));

	/* Check if 100% voltage sets energy too capacity */
	bec_set_voltage_V(&bec, 400);
	bec_set_current_A(&bec, 1);
	bec_update(&bec, update_interval_ms);
	assert(bec._energy_counts == capacity_counts);

	printf("bec_get_soc_pct(&bec): %f\n", bec_get_soc_pct(&bec));
	assert(cmp_floats_with_epsilon(bec_get_soc_pct(&bec), 100.0, 0.1));
}

void leaf_can_filter_test()
{
	int i;

	struct leaf_can_filter fi;
	struct leaf_can_filter_frame frame;
	struct bite bi;
	
	frame.id = 1468U;
	frame.len = 8U;
	frame.data[0] = 0;
	frame.data[1] = 0;
	frame.data[2] = 0;
	frame.data[3] = 0;
	frame.data[4] = 0;
	frame.data[5] = 0;
	frame.data[6] = 0;
	frame.data[7] = 0;
	
	leaf_can_filter_init(&fi);
	leaf_can_filter_process_frame(&fi, &frame);
	leaf_can_filter_update(&fi);

	for (i = 0; i < 8; i++) {
		printf("0x%02X ", frame.data[i]);
	}

	printf("\n");

	bite_init(&bi);
	bite_set_buf(&bi, frame.data, frame.len);
	bite_begin(&bi, 16, 4, BITE_ORDER_DBC_1);
	assert(bite_read(&bi) == 12);
	bite_end(&bi);
}

int main()
{
	leaf_can_filter_test();
	bec_test();
	return 0;
}
