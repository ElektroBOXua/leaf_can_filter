#include "leaf_can_filter_hal.h"

struct leaf_can_filter filter;

void setup()
{
	leaf_can_filter_hal_init();

	leaf_can_filter_init(&filter);
}

void loop()
{
	struct leaf_can_filter_frame frame;

	uint32_t delta_time_ms = leaf_can_filter_hal_get_delta_time_ms();
	leaf_can_filter_hal_update(delta_time_ms);
	leaf_can_filter_update(&filter);

	if (leaf_can_filter_hal_recv_frame(0, &frame)) {
		leaf_can_filter_process_frame(&filter, &frame);
		leaf_can_filter_hal_send_frame(1, &frame);
	}
	
	if (leaf_can_filter_hal_recv_frame(1, &frame)) {
		leaf_can_filter_process_frame(&filter, &frame);
		leaf_can_filter_hal_send_frame(0, &frame);
	}
}
