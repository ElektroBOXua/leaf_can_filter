#include "leaf_can_filter_fs.h"
#include "leaf_can_filter_web.h"
#include "leaf_can_filter_hal.h"

/******************************************************************************
 * DELTA TIME
 *****************************************************************************/
struct delta_time {
	uint32_t timestamp_prev;
};

void delta_time_init(struct delta_time *self)
{
	self->timestamp_prev = 0;
}

uint32_t delta_time_update_ms(struct delta_time *self, uint32_t timestamp)
{
	uint32_t delta_time_ms = 0;

	delta_time_ms        = timestamp - self->timestamp_prev;
	self->timestamp_prev = timestamp;

	return delta_time_ms;
}

/******************************************************************************
 * GLOBALS
 *****************************************************************************/
struct delta_time dt;
struct leaf_can_filter filter;

void leaf_can_filter_web_task(void *pv_parameters)
{
	static struct delta_time dt;
	delta_time_init(&dt);

	while(1) {
		/* TODO valid arguments */
		leaf_can_filter_web_update(&filter,
					  delta_time_update_ms(&dt, millis()));
		vTaskDelay(0);
	}
}

void setup()
{
	delta_time_init(&dt);

	leaf_can_filter_hal_init();
	leaf_can_filter_init(&filter);

	/* Init fs after filter init, so saved settings may be restored */
	leaf_can_filter_fs_init(&filter);

	/* Init web interface */
	leaf_can_filter_web_init(&filter);
	xTaskCreate(leaf_can_filter_web_task, "web", 10000, NULL, 1, NULL);
}

void loop()
{
	struct leaf_can_filter_frame frame;
	uint32_t delta_time_ms = delta_time_update_ms(&dt, millis());

	leaf_can_filter_hal_update(delta_time_ms);
	leaf_can_filter_update(&filter, delta_time_ms);

	if (leaf_can_filter_hal_recv_frame(0, &frame)) {
		leaf_can_filter_process_frame(&filter, &frame);
		leaf_can_filter_hal_send_frame(1, &frame);
	}
	
	if (leaf_can_filter_hal_recv_frame(1, &frame)) {
		leaf_can_filter_process_frame(&filter, &frame);
		leaf_can_filter_hal_send_frame(0, &frame);
	}

	leaf_can_filter_fs_update(&filter, delta_time_ms);
}
