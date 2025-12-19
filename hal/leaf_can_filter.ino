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
SemaphoreHandle_t _filter_mutex; /* prevent filter update from other tasks
				    during loop */

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

/* Look for software reset sequence (climate control button pressed multiple
 * 				     times in a certain period of time) */
void leaf_can_filter_check_softreset_sequence()
{
	/* Stuff to reset cpu via leaf interface
	 * Will also reset wifi */
	static bool reset_trigger = false;
	static uint32_t reset_trigger_counter = 0u;
	static uint32_t reset_trigger_timer = 0u;

	/* Count climate control button presses */
	if (self->clim_ctl_btn_alert != reset_trigger) {
		/* Count CC buttons presses */
		reset_trigger = self->clim_ctl_btn_alert;
		reset_trigger_counter++;
		reset_trigger_timer = 0u;
	}

	/* If no CC buttons been pressed in past 5s */
	if (reset_trigger_timer <= 5000u) {
		reset_trigger_timer += delta_time_ms;
	} else {
		/* Reset counter */
		reset_trigger_counter = 0u;
	}

	/* If CC buttons was pressed 10 times in past 5 seconds */
	if (reset_trigger_counter >= 10u) {
		ESP.restart();
	}
}

void setup()
{
	delta_time_init(&dt);

	leaf_can_filter_hal_init();

	_filter_mutex = xSemaphoreCreateMutex();
	if (_filter_mutex == NULL) {
		assert(0);
		while(true);
	}

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

	// Acquire the mutex before accessing 'filter'
	// Block indefinitely until the mutex is available
	if (xSemaphoreTake(_filter_mutex, portMAX_DELAY) == pdTRUE) {
		// --- CRITICAL SECTION START ---

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

		leaf_can_filter_check_softreset_sequence();

		// --- CRITICAL SECTION END ---

		// Release the mutex after all operations on 'filter' are complete
		xSemaphoreGive(_filter_mutex);
	} else {
		// This case typically indicates a severe error, such as the scheduler
		// not running or an issue with the mutex itself.
		// In a robust system, you might log this or handle it appropriately.
	}

	vTaskDelay(0);
}
