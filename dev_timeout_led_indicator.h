#pragma once

#include <stdbool.h>
#include <stdint.h>

/******************************************************************************
 * CLASS
 *****************************************************************************/
enum dev_timeout_led_indicator_state {
	DEV_TIMEOUT_LED_INDICATOR_STATE_INIT,
	DEV_TIMEOUT_LED_INDICATOR_STATE_CONFIG_INVALID,
	DEV_TIMEOUT_LED_INDICATOR_STATE_TIMEOUT,
	DEV_TIMEOUT_LED_INDICATOR_STATE_NORMAL,

	DEV_TIMEOUT_LED_INDICATOR_STATE_BLINK_0, /* 200ms */
	DEV_TIMEOUT_LED_INDICATOR_STATE_BLINK_1, /* 50ms */
	DEV_TIMEOUT_LED_INDICATOR_STATE_WAIT, /* 750ms */

	DEV_TIMEOUT_LED_INDICATOR_STATE_IDLE
};


/* Yellow light - configuration stage */
/* Red light    - configuration invalid */
/* Red light blink 100ms - dev timeout (number of blinks indicates CAN device) 
 * 	500ms blink delay between each device */
/* Green light - everything is ok */
struct dev_timeout_led_indicator_color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct dev_timeout_led_indicator {
/* private: */
	uint8_t _state;
	bool _timeout_triggered;

	/* binary, each bit represent one device (8 dev max) */
	uint8_t _devices_count; /* Current dev device count */
	int32_t _dev_timer_ms[8]; /* timer for each device.
				     When below zero - timeout triggered */

	/* selected dev (for timeout report) */
	uint8_t _dev_sel;
	uint8_t _blinks; /* number of blinks */

	/* General purpose timer */
	uint32_t _timer_ms;

/* public: */
	struct dev_timeout_led_indicator_color c;
};

/******************************************************************************
 * PRIVATE
 *****************************************************************************/
int8_t _dev_timeout_led_indicator_get_next_timeout_device(
					struct dev_timeout_led_indicator *self)
{
	int8_t i = 0;
	int8_t next_device = -1;

	for (i = self->_dev_sel + 1U; i < (int8_t)self->_devices_count; i++) {
		if (self->_dev_timer_ms[i] <= 0) {
			next_device = i;
		}
	}

	return next_device;
}

/******************************************************************************
 * PUBLIC
 *****************************************************************************/
void dev_timeout_led_indicator_init(struct dev_timeout_led_indicator *self)
{
	uint8_t i = 0U;

	self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_INIT;
	self->_timeout_triggered = false;

	self->_devices_count = 0U;
	for (i = 0U; i < 8U; i++) {
		self->_dev_timer_ms[i] = 0;
	}

	self->_dev_sel = 0U;
	self->_blinks  = 0U;

	self->_timer_ms = 0U;

	self->c.r = 0U;
	self->c.g = 0U;
	self->c.b = 0U;
}

/* Set number of active devices. */
void dev_timeout_led_indicator_set_count(
					struct dev_timeout_led_indicator *self,
					uint8_t dev_count)
{
	if ((dev_count <= 8U) && !self->_devices_count) {
		self->_devices_count = dev_count;
	} else {
		self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_CONFIG_INVALID;
	}
}

/* Update timer for selected dev device, so it won't timeout. */
void dev_timeout_led_indicator_update_timer(
					struct dev_timeout_led_indicator *self,
					uint8_t device, int32_t timeout_ms)
{
	if (device <= 8U) {
		self->_dev_timer_ms[device] = timeout_ms;
	} else {
		self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_CONFIG_INVALID;
	}
}

/* Returns true if colors changed */
bool dev_timeout_led_indicator_update(struct dev_timeout_led_indicator *self,
				      uint32_t delta_time_ms)
{
	uint8_t i;
	bool colors_changed = false;

	/* For all devices */
	for (i = 0U; i < self->_devices_count; i++) {
		/* If timer is greater than zero */
		if (self->_dev_timer_ms[i] > 0) {
			self->_dev_timer_ms[i] -= (int32_t)delta_time_ms;
		} else if (!self->_timeout_triggered) {
		/* Go to timeout error state */
			self->_timeout_triggered = true;
			self->_dev_sel = i;
			self->_blinks  = 0U;
			self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_TIMEOUT;
		} else {}
	}

	switch(self->_state) {
	case DEV_TIMEOUT_LED_INDICATOR_STATE_INIT:
		if (self->_devices_count > 0U) {
			/* Yellow, initial state */
			self->c.r = 0U;
			self->c.g = 0xFFU;
			colors_changed = true;

			self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_NORMAL;
		} else {
			self->_state =
				DEV_TIMEOUT_LED_INDICATOR_STATE_CONFIG_INVALID;
		}

		break;

	case DEV_TIMEOUT_LED_INDICATOR_STATE_CONFIG_INVALID:
		self->c.r = 0xFFU;
		self->c.g = 0U;
		self->c.b = 0U;
		colors_changed = true;

		self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_NORMAL;
		break;

	case DEV_TIMEOUT_LED_INDICATOR_STATE_TIMEOUT:
		self->c.r = 0U;
		self->c.g = 0U;
		self->c.b = 0U;
		colors_changed = true;

		self->_timer_ms = 0U;
		self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_WAIT;
	
		break;

	case DEV_TIMEOUT_LED_INDICATOR_STATE_WAIT:
		self->_timer_ms += delta_time_ms;

		if (self->_timer_ms >= 750U) {
			self->_timer_ms = 0U;
			self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_BLINK_0;
		}

		break;

	case DEV_TIMEOUT_LED_INDICATOR_STATE_BLINK_0:
		self->_timer_ms += delta_time_ms;

		/* Turn on red color after 200ms */
		if (self->_timer_ms >= 200U) {
			self->c.r = 0xFFU;
			colors_changed = true;

			self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_BLINK_1;
		}

		break;

	case DEV_TIMEOUT_LED_INDICATOR_STATE_BLINK_1: {
		int8_t next_device;
		
		self->_timer_ms += delta_time_ms;

		/* Turn off red color after 250ms */
		if (self->_timer_ms >= 250U) {
			self->_blinks++;
		} else {
			break;
		}

		if (self->_blinks <= self->_dev_sel)
		{
			self->c.r = 0U;
			self->c.g = 0U;
			self->c.b = 0U;
			colors_changed = true;

			self->_timer_ms = 0U;

			self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_BLINK_0;
			break;
		}

		next_device = 
		      _dev_timeout_led_indicator_get_next_timeout_device(self);

		if (next_device > -1) {
			self->_dev_sel = next_device;
			self->_blinks  = 0U;
			self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_TIMEOUT;
		} else {
			self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_NORMAL;
		}

		break;
	}

	case DEV_TIMEOUT_LED_INDICATOR_STATE_NORMAL:
		self->c.r = 0U;
		self->c.g = 0xFFU;
		self->c.b = 0U;
		colors_changed = true;

		self->_timeout_triggered = false;
		self->_state = DEV_TIMEOUT_LED_INDICATOR_STATE_IDLE;

		break;

	default:
		break;
	}

	return colors_changed;
}
