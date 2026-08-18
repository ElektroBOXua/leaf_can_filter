#ifndef   LEAF_CAN_FILTER_GUARD
#define   LEAF_CAN_FILTER_GUARD

#include <stdint.h>
#include "charge_counter.h"
#include "dev_timeout_led_indicator.h"

/******************************************************************************
 * Common
 *****************************************************************************/
/* Leaf can frame data structure */
struct leaf_can_filter_frame {
	uint32_t id;
	uint8_t  len;
	uint8_t  data[8];
};

/* Depend on struct leaf_can_filter_frame */
#include "leafspy_can_filter.h"
#include "leaf_soh_reset_fsm.h"

/******************************************************************************
 * Version sniffer to decide which vehicle version is used
 *****************************************************************************/
enum leaf_can_filter_bms_version {
	LEAF_CAN_FILTER_BMS_VERSION_UNKNOWN, /* No votes or overlap */
	LEAF_CAN_FILTER_BMS_VERSION_ZE0,
	LEAF_CAN_FILTER_BMS_VERSION_AZE0,
	LEAF_CAN_FILTER_BMS_VERSION_NV200
};

struct leaf_version_sniffer {
	/* Votes for each version. */
	uint8_t _ze0;
	uint8_t _aze0;
	uint8_t _nv200;

	/* Preditcted vehicle version */
	uint8_t _version;

	/* Timer to set version check interval */
	uint8_t _timer_ms;

	/* Guards (checks only once) */
	bool once_h50a;
	bool once_h60d;
};

void leaf_version_sniffer_init(struct leaf_version_sniffer *self)
{
	self->_ze0   = 0u;
	self->_aze0  = 0u;
	self->_nv200 = 0u;

	self->_version = LEAF_CAN_FILTER_BMS_VERSION_UNKNOWN;

	self->_timer_ms = 0u;

	self->once_h50a = false;
	self->once_h60d = false;
}

uint8_t leaf_version_sniffer_get_version(struct leaf_version_sniffer *self)
{
	return self->_version;
}

void leaf_version_sniffer_push_frame(struct leaf_version_sniffer *self,
				     struct leaf_can_filter_frame *f)
{
	switch (f->id) {
	case 0x50AU: /* BO_ 1290 x50A: 6 VCM */
		if (self->once_h50a) {
			break;
		} else {
			self->once_h50a = true;
		}

		if (f->len == 6u) {
			self->_ze0 |= 1u;
		} else if (f->len == 8u) {
			self->_aze0 |= 1u;
		} else {}

		break;

	/*case 0x5B9U:
		self->_nv200 |= 1u;*/

	case 0x60DU:
		if (self->once_h60d) {
			break;
		} else {
			self->once_h60d = true;
		}

		self->_nv200 |= 1u;

		break;

	default:
		break;
	}
}

void _leaf_version_sniffer_predict_version(struct leaf_version_sniffer *self)
{
	uint8_t new_version = LEAF_CAN_FILTER_BMS_VERSION_UNKNOWN;

	if ((self->_ze0 == 1u) && (self->_aze0 == 0u) &&
	    (self->_nv200 == 0u)) {
		new_version = LEAF_CAN_FILTER_BMS_VERSION_ZE0;
	}

	if ((self->_ze0 == 0u) && (self->_aze0 == 1u) &&
	    (self->_nv200 == 0u)) {
		new_version = LEAF_CAN_FILTER_BMS_VERSION_AZE0;
	}

	if ((self->_ze0 == 0u) && (self->_aze0 == 1u) &&
	    (self->_nv200 == 1u)) {
		new_version = LEAF_CAN_FILTER_BMS_VERSION_NV200;
	}

	self->_version = new_version;
}

void leaf_version_sniffer_step(struct leaf_version_sniffer *self,
			       uint32_t delta_time_ms)
{
	if (self->_timer_ms < 100u) {
		self->_timer_ms	+= (uint8_t)delta_time_ms;
	} else {
		self->_timer_ms -= 100u;
		_leaf_version_sniffer_predict_version(self);
	}
}

/******************************************************************************
 * BMS variables common for all leaf vehicles
 * 
 * TODO This structure should reflect original bms values.
 *   Some values are being overriden and some are not.
 *   This causes pretty much confusion when i have to get certain values.
 *   There should be separate data structure to keep overriden values.
 *****************************************************************************/
struct leaf_bms_vars {
	float voltage_V;
	float current_A;

	uint32_t remain_capacity_wh;
	uint32_t full_capacity_wh;

	uint8_t full_cap_bars;
	uint8_t remain_cap_bars;

	uint8_t soh_pct;

	float charge_power_limit_kwt;
	float max_power_for_charger_kwt;
	float temperature_c;

	/** When manual capacity is enabled,
	 * this mux will replace the original mux.
	 * But only for period of mux counter */
	uint16_t full_cap_mux_counter;
	bool     full_cap_mux;

	/* This flag forces filter to tell EVC, that battery is
	   fully charged for time period specified in
	   full_charge_flag_timer_ms */
	bool    full_charge_flag;
	int16_t full_charge_flag_timer_ms;

	/* Isolation Resistance sensor malfunction */
	bool     ir_sensor_malfunction;

	/* Isolation Resistance sensor voltage */
	uint16_t ir_sensor_wave_voltage_mV;
};

void leaf_bms_vars_init(struct leaf_bms_vars *self)
{
	self->voltage_V = 0.0f;
	self->current_A = 0.0f;

	self->remain_capacity_wh = 0u;
	self->full_capacity_wh   = 0u;

	self->full_cap_bars   = 0u;
	self->remain_cap_bars = 0u;

	self->soh_pct = 0u;

	self->charge_power_limit_kwt = 0.0f;
	self->max_power_for_charger_kwt = 0.0f;
	self->temperature_c = 0.0f;

	self->full_cap_mux_counter = 0u;
	self->full_cap_mux         = true;

	self->full_charge_flag = false;
	self->full_charge_flag_timer_ms = 0u;

	self->ir_sensor_malfunction      = false;
	self->ir_sensor_wave_voltage_mV  = 0u;
}

/******************************************************************************
 * Main data structures
 *****************************************************************************/
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

	/* bms version override */
	uint8_t bms_version_override;

	/* Filter isolation fault */
	float ir_sensor_multiplier;

	/* Must be enabled on 24kwh AZE0,
	   since BMS on this one has a hardcoded bms that tells battery is
	   dead after 24kwh is drained. Usually if the battery pack is 
	   replaced with >24kwh battery. */
	bool discharge_threshold_enabled;

	/* How low battery can go, before setting soc to 
	   discharge_threshold_soc_pct */
	float discharge_threshold_voltage_V;
};

struct leaf_can_filter {
	struct chgc _chgc; /* Charge counter */

	struct leaf_can_filter_settings settings;
	struct leaf_bms_vars _bms_vars;
	struct leafspy_can_filter lscfi;

	struct leaf_version_sniffer lvs;

	/* Experimental, test purposes only (replaces LBC01 byte by idx)*/
	uint8_t filter_leafspy_idx;
	uint8_t filter_leafspy_byte;

	/* Vehicle ON/OFF status */
	bool vehicle_is_on;

	/* Climate control */
	bool clim_ctl_recirc;
	bool clim_ctl_fresh_air;
	bool clim_ctl_btn_alert;

	/* Soh reset FSM */
	struct lcf_sr soh_rst_fsm;

	/* Counts how much time we spent being undercharged,
	 * Will reset after 2min (hardcoded) */
	uint32_t undercharge_counter_ms;
};

/******************************************************************************
 * Private
 *****************************************************************************/
static uint8_t crctable[256] = {
    0,	 133, 143, 10,	155, 30,  20,  145, 179, 54,  60,  185, 40,  173, 167,
    34,	 227, 102, 108, 233, 120, 253, 247, 114, 80,  213, 223, 90,  203, 78,
    68,	 193, 67,  198, 204, 73,  216, 93,  87,	 210, 240, 117, 127, 250, 107,
    238, 228, 97,  160, 37,  47,  170, 59,  190, 180, 49,  19,	150, 156, 25,
    136, 13,  7,   130, 134, 3,	  9,   140, 29,	 152, 146, 23,	53,  176, 186,
    63,	 174, 43,  33,	164, 101, 224, 234, 111, 254, 123, 113, 244, 214, 83,
    89,	 220, 77,  200, 194, 71,  197, 64,  74,	 207, 94,  219, 209, 84,  118,
    243, 249, 124, 237, 104, 98,  231, 38,  163, 169, 44,  189, 56,  50,  183,
    149, 16,  26,  159, 14,  139, 129, 4,   137, 12,  6,   131, 18,  151, 157,
    24,	 58,  191, 181, 48,  161, 36,  46,  171, 106, 239, 229, 96,  241, 116,
    126, 251, 217, 92,	86,  211, 66,  199, 205, 72,  202, 79,	69,  192, 81,
    212, 222, 91,  121, 252, 246, 115, 226, 103, 109, 232, 41,	172, 166, 35,
    178, 55,  61,  184, 154, 31,  21,  144, 1,	 132, 142, 11,	15,  138, 128,
    5,	 148, 17,  27,	158, 188, 57,  51,  182, 39,  162, 168, 45,  236, 105,
    99,	 230, 119, 242, 248, 125, 95,  218, 208, 85,  196, 65,	75,  206, 76,
    201, 195, 70,  215, 82,  88,  221, 255, 122, 112, 245, 100, 225, 235, 110,
    175, 42,  32,  165, 52,  177, 187, 62,  28,	 153, 147, 22,	135, 2,	  8,
    141};

/* 0x85 polynominal */
void _leaf_can_filter_calc_crc8(struct leaf_can_filter_frame *frame)
{
	uint8_t i;
	uint8_t crc = 0;

	for (i = 0; i < 7; i++) {
		crc = crctable[(crc ^ ((int)frame->data[i])) % 256];
	}

	frame->data[7] = crc;
}

/* TODO enforce this conversion everywhere */
uint16_t _leaf_can_filter_wh_to_gids(uint32_t wh)
{
	return (wh + 40u) / 80u;
}

/* Multiplies SOH by user multiplier, overrides the original value.
 *  SG_ LB_Capacity_Deterioration_Rate :
 * 	33|7@1+ (1,0) [0|100] "%" Vector__XXX */
void _leaf_can_filter_x5BC_override_soh(struct leaf_can_filter *self,
					struct leaf_can_filter_frame *frame,
					uint8_t *soh_pct)
{
	float overriden = (float)*soh_pct * self->settings.soh_mul;

	if (overriden > (float)0x7Fu) {
		overriden = (float)0x7Fu;
	}

	frame->data[4] &= 0x01u; /* mask: 00000001 */
	frame->data[4] |= ((uint8_t)overriden << 1u);

	/* Leafspy */
	self->lscfi.lbc.ovd.soh = overriden;

	/* TODO do not override original read values */
	*soh_pct = (uint8_t)overriden;
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
	} else if (self->_bms_vars.full_capacity_wh > 0u) {
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
	uint8_t  soh_pct              = 0U;

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

	/* Battery temp */
	self->_bms_vars.temperature_c = (int16_t)frame->data[3] - 40;

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

		soh_pct = (uint8_t)new_soh;
	}

	/* Override SOH based on multiplier */
	_leaf_can_filter_x5BC_override_soh(self, frame, &soh_pct);

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

	/* Override leafspy amphours and soc */
	if (self->settings.capacity_override_enabled) {
		uint32_t fullcap = chgc_get_full_cap_wh(&self->_chgc);

		self->lscfi.lbc.ovd.ah = fullcap / 355.2f; /* 3.7v * 96cells */

		if (fullcap > 0u) {
			self->lscfi.lbc.ovd.soc =
			      (chgc_get_remain_cap_wh(&self->_chgc) * 100.0f) /
			       fullcap;
		}
	}

	self->_bms_vars.remain_capacity_wh  = remain_capacity_gids;
	self->_bms_vars.remain_capacity_wh *= 80U;

	if (full_cap_bars_mux) {
		self->_bms_vars.full_cap_bars = cap_bars;
	} else {
		self->_bms_vars.remain_cap_bars = cap_bars;
	}

	self->_bms_vars.soh_pct = soh_pct;
}

void _leaf_can_filter_aze0_x5BC(struct leaf_can_filter *self,
				struct leaf_can_filter_frame *frame)
{
	const uint8_t temp_lut[13] = {25, 28, 31, 34, 37, 50, 63,
				      76, 80, 82, 85, 87, 90};

	/* capacity_gids will show either full or remaining capacity
	 * based on this mux */
	bool full_capacity_mux = ((frame->data[5U] & 0x10U) > 0U);
	uint16_t capacity_gids = 0U;

	uint8_t soh_pct = 0U;

	bool     full_cap_bars_mux  = false; /* cap_bars: full or remain */
 	uint8_t  cap_bars           = 0U;

	/* SG_ LB_Remain_Capacity :
	 * 	7|10@0+ (1,0) [0|500] "gids" Vector__XXX */
	capacity_gids = ((uint16_t)frame->data[0] << 2u) |
			((uint16_t)frame->data[1] >> 6u);

	/* Battery temp */
	/*self->_bms_vars.temperature_c = (float)frame->data[3] * 0.416667f; */
	self->_bms_vars.temperature_c =
		(int16_t)temp_lut[(frame->data[3] / 20u)] - 40;

	/* SG_ LB_Capacity_Deterioration_Rate :
	 * 	33|7@1+ (1,0) [0|100] "%" Vector__XXX */
	soh_pct = frame->data[4] >> 1u;

	/* Alternate full capacity mux manually
	 * This is required, because not all batteries has
	 * this mux (24kwh does not). They report remaining capacity only.
	 * 24kwh batteries have its full capacity set implicitly.
	 * We do not care and set it explicitly, so bars on
	 * display will update. But we do it only for short period of time.
	 * It looks like 24kwt leafs do not fully understand this behaviour
	 * and start to alternate display KM as well */
	if (self->settings.capacity_override_enabled &&
	    self->_bms_vars.full_cap_mux_counter < 10u) {
		self->_bms_vars.full_cap_mux_counter++;
		self->_bms_vars.full_cap_mux = !self->_bms_vars.full_cap_mux;

		/* Override local capacity mux (for this message) */
		full_capacity_mux = self->_bms_vars.full_cap_mux;
		frame->data[5] &= 0xEFu; /* mask: 11101111 */
		frame->data[5] |= (uint8_t)full_capacity_mux << 4u;
	}

	/* Override SOH based on multiplier */
	_leaf_can_filter_x5BC_override_soh(self, frame, &soh_pct);

	full_cap_bars_mux = (frame->data[4] & 0x01u);
	cap_bars          = (frame->data[2] & 0xFFu);

	if (self->settings.capacity_override_enabled ||
	    (self->settings.soh_mul != 1.0f)) {
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

		/*if ((frame->data[5] >> 5) == 1u) {
			... check if capacity drop flag is active
		}*/
	}

	if (self->settings.capacity_override_enabled &&
	    self->settings.discharge_threshold_enabled) {
		/* Ignore capacity drop flag. Normally only required
		   to perform on 24kwt leaf aze0 batteries, because
		   bms sets this flag when its internal capacity counter
		   goes zero. This does not occur on >24kwh batteries by some
		   reason. TODO do not perform this on other batteries */
		frame->data[5] &= 0xDF; /* mask: 11011111 */
	}

	if (full_capacity_mux) {
		self->_bms_vars.full_capacity_wh  = capacity_gids;
		self->_bms_vars.full_capacity_wh *= 80U;
	} else {
		self->_bms_vars.remain_capacity_wh  = capacity_gids;
		self->_bms_vars.remain_capacity_wh *= 80U;
	}

	/* Override leafspy amphours and soc */
	if (self->settings.capacity_override_enabled) {
		uint32_t fullcap = chgc_get_full_cap_wh(&self->_chgc);

		self->lscfi.lbc.ovd.ah = fullcap / 355.2f; /* 3.7v * 96cells */

		if (fullcap > 0u) {
			self->lscfi.lbc.ovd.soc =
			      (chgc_get_remain_cap_wh(&self->_chgc) * 100.0f) /
			       fullcap;
		}
	}

	if (full_cap_bars_mux) {
		self->_bms_vars.full_cap_bars = cap_bars;
	} else {
		self->_bms_vars.remain_cap_bars = cap_bars;
	}

	self->_bms_vars.soh_pct = soh_pct;
}

/** Controls discharge, when battery voltage drops below certain
 * threshold. Currently it only drops capacity counter to 1%, so it tells
 * the vehicle that it has to limit its power */
void _leaf_can_filter_control_discharge(struct leaf_can_filter *self,
					uint32_t delta_time_ms)
{
	/* Flag that tells if discharge threshold was reached */
	bool discharge_threshold_reached = false;

	/* If capacity override_enabled and
	   discharge threshold enabled and
	   voltage is less than discharge threshold voltage */
	if (self->settings.capacity_override_enabled &&
	    self->settings.discharge_threshold_enabled &&
	    (self->_bms_vars.voltage_V <=
	     self->settings.discharge_threshold_voltage_V)) {
		     discharge_threshold_reached = true;
	}

	if (discharge_threshold_reached) {
		/* Delay triggering due to instant voltage fluctuations
		 * (up to 2min) */
		if (self->undercharge_counter_ms > (120u * 1000u)) {
			/* Just reset (Still may trigger lately,
			 * 	       but whatever) */
			self->undercharge_counter_ms = 0u;

			/* Set manual capacity to 1% */
			chgc_set_initial_cap_kwh(&self->_chgc,
				chgc_get_full_cap_kwh(&self->_chgc) * 0.01);
		}

		self->undercharge_counter_ms += delta_time_ms;
	} else {
		/* Reset counter if above threshold */
		self->undercharge_counter_ms = 0u;
	}
}

/* Filter HVBAT frames */
void _leaf_can_filter(struct leaf_can_filter *self,
		      struct leaf_can_filter_frame *frame)
{
	uint8_t version;

	leaf_version_sniffer_push_frame(&self->lvs, frame);

	if (self->settings.bms_version_override > 0u) {
		version = self->settings.bms_version_override;
	} else {
		version = leaf_version_sniffer_get_version(&self->lvs);
	}

	switch (frame->id) {
	/* BO_ 1468 x5BC: 8 HVBAT */
	case 1468U: {
		/* If LBC not booted up - exit */
		if (frame->data[0U] == 0xFFU) {
			/* Reset some states before boot */
			self->_bms_vars.full_cap_mux_counter = 0u;
			self->_bms_vars.full_cap_mux         = true;
			break;
		}

		if (version == (uint8_t)LEAF_CAN_FILTER_BMS_VERSION_ZE0) {
			_leaf_can_filter_ze0_x5BC(self, frame);
		}

		if (version == (uint8_t)LEAF_CAN_FILTER_BMS_VERSION_AZE0) {
			_leaf_can_filter_aze0_x5BC(self, frame);
		}

		/* Temporarily same as AZE0 */
		if (version == (uint8_t)LEAF_CAN_FILTER_BMS_VERSION_NV200) {
			_leaf_can_filter_aze0_x5BC(self, frame);
		}

		break;
	}

	/* BO_ 475 x1DB: 8 HVBAT
	 * 10ms */
	case 475U: {
		/* Important WARNING:
		 * Logs from nv200 (2019 year) does not have this message.
		 * (40kwt batt). TODO find correct messages. TODO version */
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

		/* Override SOC on main dash panel
		 * (if override, AZE0 and not division by zero) */
		if ((self->settings.capacity_override_enabled) &&
		    (version == (uint8_t)LEAF_CAN_FILTER_BMS_VERSION_AZE0) &&
		    (self->_bms_vars.full_capacity_wh > 0u)) {
			uint32_t dash_soc =
				(self->_bms_vars.remain_capacity_wh * 100u) /
				 self->_bms_vars.full_capacity_wh;

			frame->data[4] = (uint8_t)dash_soc;
		}

		/* Set full charge flag */
		if (self->_bms_vars.full_charge_flag) {
			frame->data[3] = (frame->data[3] | 0x10);
		}

		self->_bms_vars.voltage_V = voltage_500mV / 2.0f;
		self->_bms_vars.current_A = current_500mA / 2.0f;

		/* We pass 10ms delta time
		 * (as we know that this message has 10ms period anyways) */
		_leaf_can_filter_control_discharge(self, 10u);

		/* Report voltage and current to our energy counter 
		 * (Raw values scaled by 2x) */
		chgc_set_voltage_V(&self->_chgc, voltage_500mV);
		chgc_set_current_A(&self->_chgc, current_500mA);

		/* Save remaining capacity into settings */
		self->settings.capacity_remaining_kwh =
			chgc_get_remain_cap_kwh(&self->_chgc);

		_leaf_can_filter_calc_crc8(frame);

		break;
	}

	case 0x1DCu: {
		uint16_t charge_power_limit_250w   = 0U;
		uint16_t max_power_for_charger_kwt = 0U;

		/* LB_Charge_Power_Limit */
		charge_power_limit_250w =
			((uint16_t)(frame->data[1] & 0x3Fu) << 4u) |
			((uint16_t) frame->data[2]          >> 4u);

		self->_bms_vars.charge_power_limit_kwt =
			(float)charge_power_limit_250w * 0.25f;

		/* LB_MAX_POWER_FOR_CHARGER */
		max_power_for_charger_kwt =
			((uint16_t)(frame->data[2] & 0x0Fu) << 6u) |
			((uint16_t) frame->data[3]          >> 2u);

		self->_bms_vars.max_power_for_charger_kwt =
			(float)max_power_for_charger_kwt * 0.1 - 10.0;

		break;
	}

	case 0x11A:
		if ((frame->data[1] & 0x40u) > 0u) {
			self->vehicle_is_on = true;
		} else if ((frame->data[1] & 0x80u) > 0u) {
			self->vehicle_is_on = false;
		} else {
			self->vehicle_is_on = false;
		}

		break;

	case 0x54B:
		if (frame->data[3] == 9u) {
			self->clim_ctl_recirc    = true;
			self->clim_ctl_fresh_air = false;
		} else if (frame->data[3] == (9u << 1u)) {
			self->clim_ctl_recirc    = false;
			self->clim_ctl_fresh_air = true;
		} else {}

		self->clim_ctl_btn_alert = ((frame->data[7] & 0x01u) > 0u) ?
						1u : 0u;

		break;

	case 0x55B:
		/* Override LB_SOC and set battery empty flag to 0
		   Looks like only meaningful for 24kwh aze0. */
		if ((self->settings.capacity_override_enabled) &&
		    (version == (uint8_t)LEAF_CAN_FILTER_BMS_VERSION_AZE0) &&
		    (self->_bms_vars.full_capacity_wh > 0u)) {
			uint32_t overriden =
				(self->_bms_vars.remain_capacity_wh * 999u /
				 self->_bms_vars.full_capacity_wh);

			/* SG_ LB_SOC : 7|10@0+ (1,0) [0|1000] "%+1"
			   Vector__XXX */
			frame->data[0] &= 0x00u; /* mask: 00000000 */	
			frame->data[0] |= (overriden >> 2u);
			frame->data[1] &= 0x3Fu; /* mask: 00111111 */
			frame->data[1] |= (overriden << 6u);
		}

		/* Ignore capacity empty flag, if discharge threshold mode
		   enabled */
		if (self->settings.capacity_override_enabled &&
		    self->settings.discharge_threshold_enabled) {
			/*  SG_ LB_Capacity_Empty : 55|1@1+ (1,0) [0|1]
			  "modemask" Vector__XXX */
			frame->data[6] &= 0x7Fu; /* mask: 01111111 */
		}

		self->_bms_vars.ir_sensor_wave_voltage_mV =
				((uint16_t)frame->data[4] << 2u) |
				((uint16_t)frame->data[5] >> 6u);

		self->_bms_vars.ir_sensor_malfunction = frame->data[5] & 1u;

		if (self->settings.ir_sensor_multiplier != 1.0f) {
			uint16_t overriden =
				(self->_bms_vars.ir_sensor_wave_voltage_mV *
				 self->settings.ir_sensor_multiplier);

			/* Based on DBC
			  the max value lies within first 10 bits... */
			if (overriden > 1023u)
				overriden = 1023u;

			frame->data[4] &= 0x00u; /* mask: 00000000 */
			frame->data[4] |= (overriden >> 2u);
			frame->data[5] &= 0x3Fu; /* mask: 00111111 */
			frame->data[5] |= (overriden << 6u);
		}

		/* For every bit there's 4.8828125 mV. (5000/1024)
		   The maximum value in millivolts is 4990 */
		self->_bms_vars.ir_sensor_wave_voltage_mV *= 4.8828125f;

		_leaf_can_filter_calc_crc8(frame);

		break;
	
	default:
		break;
	}
}

void _leaf_can_filter_update(struct leaf_can_filter *self,
			     uint32_t delta_time_ms)
{
	/* If _bms_vars.full_charge_flag is enabled
	   Decrease its timer. If timer goes below zero - reset the flag */
	if (self->_bms_vars.full_charge_flag) {
		if (self->_bms_vars.full_charge_flag_timer_ms > 0) {
			self->_bms_vars.full_charge_flag_timer_ms -= delta_time_ms;
		} else {
			self->_bms_vars.full_charge_flag = false;
		}
	}

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
	s->bms_version_override = 0u;

	s->ir_sensor_multiplier = 1.0f;

	s->discharge_threshold_enabled = false;
	s->discharge_threshold_voltage_V = 310;

	/* Other (some settings may depend on FS) */
	chgc_init(&self->_chgc);
	chgc_set_update_interval_ms(&self->_chgc, 10U);
	chgc_set_multiplier(&self->_chgc, 2U);
	leaf_bms_vars_init(&self->_bms_vars);

	/* LeafSpy ISO-TP filtering for OBD-II interface*/
	leafspy_can_filter_init(&self->lscfi);

	/* ... */
	leaf_version_sniffer_init(&self->lvs);

	/* Experimental, test purposes only (replaces LBC01 byte by idx)*/
	self->filter_leafspy_idx  = 0u;
	self->filter_leafspy_byte = 0u;

	/* Vehicle ON/OFF status */
	self->vehicle_is_on = false;

	/* Climate control */
	self->clim_ctl_recirc    = false;
	self->clim_ctl_fresh_air = false;
	self->clim_ctl_btn_alert = false;

	lcf_sr_init(&self->soh_rst_fsm);

	self->undercharge_counter_ms = 0u;
}

void leaf_can_filter_process_frame(struct leaf_can_filter *self,
				   struct leaf_can_filter_frame *frame)
{
	/* Only process frame when bypass is disabled. */
	if (self->settings.bypass == false) {
		_leaf_can_filter(self, frame);

		/* Filter LeafSpy messages */
		if (self->settings.filter_leafspy &&
			/* We not filter leafspy messages for NV200, yet */
		    (leaf_version_sniffer_get_version(&self->lvs) !=
			(uint8_t)LEAF_CAN_FILTER_BMS_VERSION_NV200)
		    ) {
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
		leaf_version_sniffer_step(&self->lvs, delta_time_ms);
		lcf_sr_step(&self->soh_rst_fsm, delta_time_ms);
	}
}

#endif /* LEAF_CAN_FILTER_GUARD */
