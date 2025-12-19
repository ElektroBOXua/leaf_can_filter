#ifndef   LEAF_CAN_FILTER_GUARD
#define   LEAF_CAN_FILTER_GUARD

#include <stdint.h>
#include "charge_counter.h"
#include "dev_timeout_led_indicator.h"

/******************************************************************************
 * CLASS
 *****************************************************************************/
struct leaf_bms_vars {
	float voltage_V;
	float current_A;

	uint32_t remain_capacity_wh;
	uint32_t full_capacity_wh;

	uint8_t full_cap_bars;
	uint8_t remain_cap_bars;

	uint8_t soh;
};

void leaf_bms_vars_init(struct leaf_bms_vars *self)
{
	self->voltage_V = 0.0f;
	self->current_A = 0.0f;

	self->remain_capacity_wh = 0u;
	self->full_capacity_wh   = 0u;

	self->full_cap_bars   = 0u;
	self->remain_cap_bars = 0u;

	self->soh = 0u;
}

/******************************************************************************
 * CLASS
 *****************************************************************************/
enum leaf_can_filter_bms_version {
	LEAF_CAN_FILTER_BMS_VERSION_UNKNOWN,
	LEAF_CAN_FILTER_BMS_VERSION_ZE0,
	LEAF_CAN_FILTER_BMS_VERSION_AZE0
};

/* Leaf can frame data structure */
struct leaf_can_filter_frame {
	uint32_t id;
	uint8_t  len;
	uint8_t  data[8];
};

/* leafSpy can filter depend on struct leaf_can_filter_frame */
#include "leafspy_can_filter.h"

struct leaf_can_filter_settings {
	/* Enable LeafSpy filtering */
	bool filter_leafspy;

	/* Bypass filtering completely */
	bool bypass;

	/* Override bms capacity (enables energy counter) */
	bool  capacity_override_enabled;
	float capacity_override_kwh;
	float capacity_remaining_kwh;
	float capacity_full_voltage_V;

	float soh_mul;
};

struct leaf_can_filter {
	struct chgc _chgc; /* Charge counter */

	struct leaf_can_filter_settings settings;
	struct leaf_bms_vars _bms_vars;
	struct leafspy_can_filter lscfi;

	uint8_t _version;

	/* Experimental, test purposes only (replaces LBC01 byte by idx)*/
	uint8_t filter_leafspy_idx;
	uint8_t filter_leafspy_byte;

	/* Vehicle ON/OFF status */
	bool vehicle_is_on;

	/* Climate control */
	bool clim_ctl_recirc;
	bool clim_ctl_fresh_air;
	bool clim_ctl_btn_alert;
};

/******************************************************************************
 * PRIVATE
 *****************************************************************************/
/* TODO enforce this conversion everywhere */
uint16_t _leaf_can_filter_wh_to_gids(uint32_t wh)
{
	return (wh + 40u) / 80u;
}

uint8_t _leaf_can_filter_aze0_x5BC_get_cap_bars_overriden(
	struct leaf_can_filter *self, bool full_cap_bars_mux, uint8_t soh_pct)
{
	uint8_t overriden;

	/* BARS is a u8 fixed point float
	 * where 4bit MSB has 1.25 bits per bar (15 / 12 == 1.25)
	 * and   4bit LSB is a fraction
	 * BARS from MSB can be calculated by the next (ceiling) formula:
	 * 	(((0..15) * 100u) + 124u) / 125u;
	 * 
	 * Since 12 is the highest possible bar count and it can not have
	 * fraction (there is no 13th bar to round to) the actual
	 * max value is 240u (11110000b) */
	if (full_cap_bars_mux) {
		uint16_t bars = (240u * (uint16_t)soh_pct) / 100u;

		if (bars > 240u) {
			bars = 240u;
		}

		overriden = (uint8_t)bars;
	} else if (self->_bms_vars.remain_capacity_wh > 0u) {
		uint16_t bars = ((self->_bms_vars.remain_capacity_wh *
				  240u) /
				 self->_bms_vars.full_capacity_wh);

		if (bars > 240u) {
			bars = 240u;
		}

		overriden = (uint8_t)bars;
	} else {
		overriden = 0u; /* Division by zero */
	}

	return overriden;
}

/* OLDER < 2012 cars */
void _leaf_can_filter_ze0_x5BC(struct leaf_can_filter *self,
			       struct leaf_can_filter_frame *frame)
{
	uint16_t remain_capacity_gids = 0U;
	uint16_t full_capacity        = 0U;
	uint16_t soh_pct              = 0U;

	bool     full_cap_bars_mux = false; /* cap_bars: full or remain */
 	uint8_t  cap_bars          = 0U;

	/* SG_ LB_Remain_Capacity :
	 * 	7|10@0+ (1,0) [0|500] "gids" Vector__XXX */
	remain_capacity_gids = ((uint16_t)frame->data[0] << 2u) |
			       ((uint16_t)frame->data[1] >> 6u);

	/* SG_ LB_New_Full_Capacity :
	 * 	13|10@0+ (80,20000) [20000|24000] "wh" Vector__XXX */
	full_capacity = ((uint16_t)(frame->data[1] & 0x3Fu) << 4u) |
			((uint16_t) frame->data[2]          >> 4u);

	/* SG_ LB_Capacity_Deterioration_Rate :
	 * 	33|7@1+ (1,0) [0|100] "%" Vector__XXX */
	soh_pct = frame->data[4] >> 1u;

	full_cap_bars_mux = frame->data[4] & 0x01u;
	cap_bars          = frame->data[2] & 0x0Fu;

	if (self->settings.capacity_override_enabled) {
		uint16_t overriden;
		uint32_t new_soh;

		/* Override remain capacity */
		overriden = chgc_get_remain_cap_wh(&self->_chgc) / 80U;
		frame->data[0] &= 0x00u; /* mask: 00000000 */	
		frame->data[0] |= (overriden >> 2u);
		frame->data[1] &= 0x3Fu; /* mask: 00111111 */
		frame->data[1] |= (overriden << 6u);

		/* TODO do not override read values */
		remain_capacity_gids = overriden;

		/* Override Full capacity */
		/* overriden = chgc_get_full_cap_wh(&self->_chgc) / 80U; */

		/* We'll just set 24000wh. Since there's no way to get higher.
		 * And we'll use SOH to adjust the actual value lower.
		 * In old leafs SOH+fullcap is responsible for capacity bars
		 * on display primarily and the actual full capacity bars
		 * looks like have no effect (as far as i can say) */
		overriden = (24000u - 20000u) / 80u;
		frame->data[1] &= 0xC0u; /* mask: 11000000 */
		frame->data[1] |= (overriden >> 4u);
		frame->data[2] &= 0x0Fu; /* mask: 00001111 */
		frame->data[2] |= (overriden << 4u);
		/* TODO do not override read values */
		full_capacity = overriden;

		/* We need to pre-override SOH to adjust full capacity,
		 * as well as remaining capacity bars on display */
		new_soh = (uint32_t)chgc_get_full_cap_wh(&self->_chgc) * 100u /
			  24000u;

		if (new_soh > 0x7Fu) {
			new_soh = 0x7Fu;
		}

		soh_pct = (uint16_t)new_soh;
	}

	/* Override SOH (again, but manually)
	 *  SG_ LB_Capacity_Deterioration_Rate :
	 * 	33|7@1+ (1,0) [0|100] "%" Vector__XXX */
	if (self->settings.capacity_override_enabled) {
		float overriden = (float)soh_pct * self->settings.soh_mul;

		if (overriden > (float)0x7Fu) {
			overriden = (float)0x7Fu;
		}

		frame->data[4] &= 0x01u; /* mask: 00000001 */
		frame->data[4] |= ((uint8_t)overriden << 1u);

		/* TODO do not override original read values */
		soh_pct = (uint8_t)overriden;
	}

	/* SG_ LB_Remaining_Charge_Segment m0 :
	 * 	16|4@1+ (1,0) [0|12] "dash bars" Vector__XXX */
	/* SG_ LB_Remaining_Capacity_Segment m1 :
	 * 	16|4@1+ (1,0) [0|12] "dash bars" Vector__XXX */
	if (self->settings.capacity_override_enabled) {
		uint32_t overriden;

		if (full_cap_bars_mux) {
			overriden = (12u * (uint32_t)soh_pct) / 100u;
		} else {
			/* 1. Calculate the Divisor's half */
			uint16_t divisor_half =
				self->_bms_vars.full_capacity_wh / 2u;

			/* 2. Rescale value (by 12 bars) */
			overriden = (self->_bms_vars.remain_capacity_wh * 12u);

			/* 3. Add divisor_half to perform rounding */
			overriden += divisor_half;

			/* 4. Get bars rounded */
			if (self->_bms_vars.full_capacity_wh > 0u) {
				overriden /= self->_bms_vars.full_capacity_wh;
			} else {
				overriden = 0u; /* Division by zero */
			}
		}

		/* Clamp bars to 12 (max) */
		if (overriden > 12u) {
			overriden = 12u;
		}

		frame->data[2] &= 0xF0; /* mask: 11110000 */
		frame->data[2] |= (uint8_t)overriden;

		/* TODO do not override original read values */
		cap_bars = (uint8_t)overriden;
	}

	self->_bms_vars.full_capacity_wh  = full_capacity;
	self->_bms_vars.full_capacity_wh *= 80U;
	self->_bms_vars.full_capacity_wh += 20000U;

	/* Override leafspy amphours */
	if (self->settings.capacity_override_enabled) {
		self->lscfi.lbc.ovd.ah = chgc_get_full_cap_wh(&self->_chgc)
					 / 355.2f; /* 3.7v * 96cells */
	}

	self->_bms_vars.remain_capacity_wh  = remain_capacity_gids;
	self->_bms_vars.remain_capacity_wh *= 80U;

	if (full_cap_bars_mux) {
		self->_bms_vars.full_cap_bars = cap_bars;
	} else {
		self->_bms_vars.remain_cap_bars = cap_bars;
	}

	self->_bms_vars.soh = soh_pct;
}

void _leaf_can_filter_aze0_x5BC(struct leaf_can_filter *self,
				struct leaf_can_filter_frame *frame)
{
	/* capacity_gids will show either full or remaining capacity
	 * based on this mux */
	bool full_capacity_mux = ((frame->data[5U] & 0x10U) > 0U);
	uint16_t capacity_gids = 0U;

	uint16_t soh_pct = 0U;

	bool     full_cap_bars_mux  = false; /* cap_bars: full or remain */
 	uint8_t  cap_bars           = 0U;


	/* SG_ LB_Remain_Capacity :
	 * 	7|10@0+ (1,0) [0|500] "gids" Vector__XXX */
	capacity_gids = ((uint16_t)frame->data[0] << 2u) |
			((uint16_t)frame->data[1] >> 6u);

	/* SG_ LB_Capacity_Deterioration_Rate :
	 * 	33|7@1+ (1,0) [0|100] "%" Vector__XXX */
	soh_pct = frame->data[4] >> 1u;

	/* Override SOH */
	if (self->settings.capacity_override_enabled) {
		float overriden = (float)soh_pct * self->settings.soh_mul;

		if (overriden > (float)0x7Fu) {
			overriden = (float)0x7Fu;
		}

		frame->data[4] &= 0x01u; /* mask: 00000001 */
		frame->data[4] |= ((uint8_t)overriden << 1u);

		/* TODO do not override original read values */
		soh_pct = (uint8_t)overriden;
	}

	full_cap_bars_mux = (frame->data[4] & 0x01u);
	cap_bars          = (frame->data[2] & 0xFFu);

	if (self->settings.capacity_override_enabled) {
		uint8_t overriden =
			_leaf_can_filter_aze0_x5BC_get_cap_bars_overriden(
				self, full_cap_bars_mux, soh_pct
			);

		frame->data[2] &= 0x00; /* mask: 00000000 */
		frame->data[2] |= (uint8_t)overriden;

		/* TODO do not override original read values */
		cap_bars = (uint8_t)overriden;
	}

	if (self->settings.capacity_override_enabled) {
		uint16_t overriden;

		if (full_capacity_mux) {
			/* Override full capacity */
			overriden = _leaf_can_filter_wh_to_gids(
				chgc_get_full_cap_wh(&self->_chgc));

			/* TODO do not override original read values */
			capacity_gids = overriden;
		} else {
			/* Override remaining capacity */
			overriden = _leaf_can_filter_wh_to_gids(
				chgc_get_remain_cap_wh(&self->_chgc));

			/* TODO do not override original read values */
			capacity_gids = overriden;
		}

		frame->data[0] &= 0x00u; /* mask: 00000000 */
		frame->data[0] |= (overriden >> 2u);
		frame->data[1] &= 0x3Fu; /* mask: 00111111 */
		frame->data[1] |= (overriden << 6u);
	}

	if (full_capacity_mux) {
		self->_bms_vars.full_capacity_wh  = capacity_gids;
		self->_bms_vars.full_capacity_wh *= 80U;
	} else {
		self->_bms_vars.remain_capacity_wh  = capacity_gids;
		self->_bms_vars.remain_capacity_wh *= 80U;
	}

	/* Override leafspy amphours */
	if (self->settings.capacity_override_enabled) {
		self->lscfi.lbc.ovd.ah = chgc_get_full_cap_wh(&self->_chgc)
					 / 355.2f; /* 3.7v * 96cells */
	}

	if (full_cap_bars_mux) {
		self->_bms_vars.full_cap_bars = cap_bars;
	} else {
		self->_bms_vars.remain_cap_bars = cap_bars;
	}

	self->_bms_vars.soh = soh_pct;
}

/* Filter HVBAT frames */
void _leaf_can_filter(struct leaf_can_filter *self,
		      struct leaf_can_filter_frame *frame)
{
	switch (frame->id) {
	case 0x50AU: /* BO_ 1290 x50A: 6 VCM */
		if (frame->len == 6u) {
			self->_version = LEAF_CAN_FILTER_BMS_VERSION_ZE0;
		} else if (frame->len == 8u) {
			self->_version = LEAF_CAN_FILTER_BMS_VERSION_AZE0;
		} else {}

		break;

	/* BO_ 1468 x5BC: 8 HVBAT */
	case 1468U: {
		/* If LBC not booted up - exit */
		if (frame->data[0U] == 0xFFU) {
			break;
		}

		if (self->_version ==
		    (uint8_t)LEAF_CAN_FILTER_BMS_VERSION_ZE0) {
			_leaf_can_filter_ze0_x5BC(self, frame);
		}

		if (self->_version ==
		    (uint8_t)LEAF_CAN_FILTER_BMS_VERSION_AZE0) {
			_leaf_can_filter_aze0_x5BC(self, frame);
		}

		break;
	}

	/* BO_ 475 x1DB: 8 HVBAT */
	case 475U: {
		uint16_t voltage_500mV = 0U;
		int16_t  current_500mA = 0;

		/* SG_ LB_Total_Voltage :
		 * 	23|10@0+ (0.5,0) [0|450] "V" Vector__XXX */
		voltage_500mV = ((uint16_t)frame->data[2] << 2u) |
				((uint16_t)frame->data[3] >> 6u);

		/* Not booted up - exit */
		if (voltage_500mV == 1023U) {
			break;
		}

		/* SG_ LB_Current :
		 * 	7|11@0+ (0.5,0) [-400|200] "A" Vector__XXX */
		current_500mA = ((uint16_t)frame->data[0] << 3u) |
				((uint16_t)frame->data[1] >> 5u);

		/* Check sign, if present - invert MSB */
		if ((frame->data[0] & 0x80u) > 0u) {
			unsigned msb_mask = (0xFFFFu << 11u);
			current_500mA |= (int16_t)msb_mask;
		}

		self->_bms_vars.voltage_V = voltage_500mV / 2.0f;
		self->_bms_vars.current_A = current_500mA / 2.0f;

		/* Report voltage and current to our energy counter 
		 * (Raw values scaled by 2x) */
		chgc_set_voltage_V(&self->_chgc, voltage_500mV);
		chgc_set_current_A(&self->_chgc, current_500mA);

		/* Save remaining capacity into settings */
		self->settings.capacity_remaining_kwh =
			chgc_get_remain_cap_kwh(&self->_chgc);

		break;
	}

	case 0x11A:
		if ((frame->data[1] & 0x40u) > 0) {
			self->vehicle_is_on = true;
		} else if (frame->data[1] & 0x80u) {
			self->vehicle_is_on = false;
		} else {
			self->vehicle_is_on = false;
		}

		break;

	case 0x54B:
		if (frame->data[3] == 9u) {
			self->clim_ctl_recirc    = true;
			self->clim_ctl_fresh_air = false;
		} else if (frame->data[3] == 9u << 1) {
			self->clim_ctl_recirc    = false;
			self->clim_ctl_fresh_air = true;
		}

		self->clim_ctl_btn_alert = ((frame->data[7] & 0x01u) > 0u) ?
						1u : 0u;

		break;
	
	default:
		break;
	}
}

void _leaf_can_filter_update(struct leaf_can_filter *self,
			     uint32_t delta_time_ms)
{
	chgc_update(&self->_chgc, delta_time_ms);
}

/******************************************************************************
 * PUBLIC
 *****************************************************************************/
void leaf_can_filter_init(struct leaf_can_filter *self)
{
	struct leaf_can_filter_settings *s = &self->settings;

	/* Settings (DEFAULT) */
	s->filter_leafspy = false;
	s->bypass         = true;

	s->capacity_override_enabled = false;
	s->capacity_override_kwh     = 0.0f;
	s->capacity_remaining_kwh    = 0.0f;
	s->capacity_full_voltage_V   = 0.0f;

	s->soh_mul = 1.0f;

	/* Other (some settings may depend on FS) */
	chgc_init(&self->_chgc);
	chgc_set_update_interval_ms(&self->_chgc, 10U);
	chgc_set_multiplier(&self->_chgc, 2U);
	leaf_bms_vars_init(&self->_bms_vars);

	/* LeafSpy ISO-TP filtering for OBD-II interface*/
	leafspy_can_filter_init(&self->lscfi);

	/* ... */
	self->_version = LEAF_CAN_FILTER_BMS_VERSION_UNKNOWN;

	/* Experimental, test purposes only (replaces LBC01 byte by idx)*/
	self->filter_leafspy_idx  = 0u;
	self->filter_leafspy_byte = 0u;

	/* Vehicle ON/OFF status */
	self->vehicle_is_on = false;

	/* Climate control */
	self->clim_ctl_recirc    = false;
	self->clim_ctl_fresh_air = false;
	self->clim_ctl_btn_alert = false;
}

void leaf_can_filter_process_frame(struct leaf_can_filter *self,
				   struct leaf_can_filter_frame *frame)
{
	/* Only process frame when bypass is disabled. */
	if (self->settings.bypass == false) {
		_leaf_can_filter(self, frame);

		/* Filter LeafSpy messages */
		if (self->settings.filter_leafspy) {
			leafspy_can_filter_process_lbc_block1_frame(
				&self->lscfi, frame);

			/* Experimental, test purposes only
			 * (replaces LBC01 byte by idx) */
			self->lscfi.filter_leafspy_idx  =
				self->filter_leafspy_idx;

			self->lscfi.filter_leafspy_byte =
				self->filter_leafspy_byte;
		}
	}
}

void leaf_can_filter_update(struct leaf_can_filter *self,
			    uint32_t delta_time_ms)
{
	/* Certain tasks should not be updated during bypass */
	if (self->settings.bypass == false) {
		_leaf_can_filter_update(self, delta_time_ms);
	}
}

#endif /* LEAF_CAN_FILTER_GUARD */
