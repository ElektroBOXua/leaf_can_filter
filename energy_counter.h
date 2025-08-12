#ifndef   ENERGY_COUNTER_GUARD
#define   ENERGY_COUNTER_GUARD

#include <stdint.h>

/******************************************************************************
 * CLASS
 *****************************************************************************/
struct bec
{
	/* Config */
	uint32_t _full_cap_wh;
	int16_t  _full_cap_voltage_V;
	uint32_t _update_interval_ms;
	
	/* Runtime */
	int16_t _voltage_V;
	int16_t _current_A;

	/* Energy counter (accumulator) */
	int64_t _cap_counts;
	
	int32_t  _update_timer_ms;
	uint32_t _full_cap_voltage_debounce_ms;
};

/******************************************************************************
 * PRIVATE
 *****************************************************************************/
int64_t _bec_get_counts_per_hour(struct bec *self)
{
	return ((1000U / self->_update_interval_ms) * 60U * 60U);
}

int64_t _bec_conv_wh_to_counts(struct bec *self, int64_t val)
{
	return val * _bec_get_counts_per_hour(self);
}

/******************************************************************************
 * INIT
 *****************************************************************************/
void bec_init(struct bec *self)
{
	/* Config */
	self->_full_cap_wh        = 0U;
	self->_full_cap_voltage_V = 0U;
	self->_update_interval_ms = 0U;

	/* Runtime */
	self->_voltage_V  = 0U;
	self->_current_A  = 0U;
	self->_cap_counts = 0;
	
	self->_update_timer_ms              = 0;
	self->_full_cap_voltage_debounce_ms = 0;
}

/******************************************************************************
 * CONFIG
 *****************************************************************************/
void bec_set_full_cap_wh(struct bec *self, uint32_t val)
{
	self->_full_cap_wh = val;
}

void bec_set_full_cap_kwh(struct bec *self, float val)
{
	bec_set_full_cap_wh(self, val * 1000.0f);
}

uint32_t bec_get_full_cap_wh(struct bec *self)
{
	return self->_full_cap_wh;
}

float bec_get_full_cap_kwh(struct bec *self)
{
	return bec_get_full_cap_wh(self) / 1000.0f;
}

void bec_set_initial_cap_kwh(struct bec *self, float val)
{
	int64_t e = (int64_t)(val * 1000.0f); /* convert to watts */

	/* Setting initial energy without update interval
	 * is undefined behaviour (TODO define) */
	assert(self->_update_interval_ms != 0U);

	e = e * _bec_get_counts_per_hour(self);

	self->_cap_counts = e;
}

void bec_set_full_cap_voltage_V(struct bec *self, int16_t val)
{
	self->_full_cap_voltage_V = val;
}

void bec_set_update_interval_ms(struct bec *self, uint32_t val)
{
	/* Changing interval is undefined behaviour (TODO define) */
	assert(self->_cap_counts == 0);

	self->_update_interval_ms = val;
	self->_update_timer_ms    = val;
}


/******************************************************************************
 * RUNTIME
 *****************************************************************************/
/* 1V/bit precision */
void bec_set_voltage_V(struct bec *self, int16_t val)
{
	/* TODO set to 0, if not called for too long after certain timeout */
	self->_voltage_V = val;
}

/* 1A/bit precision */
void bec_set_current_A(struct bec *self, int16_t val)
{
	/* TODO set to 0, if not called for too long after certain timeout */
	self->_current_A = val;
}

/* 1W/bit precision */
uint32_t bec_get_remain_cap_wh(struct bec *self)
{
	/* Divide accumulated energy to update intervals per hour */
	return (self->_cap_counts / _bec_get_counts_per_hour(self));
}

/* 1W/bit precision */
float bec_get_remain_cap_kwh(struct bec *self)
{
	return bec_get_remain_cap_wh(self) / 1000.0f;
}

float bec_get_soc_pct(struct bec *self)
{
	float result = 0.0f;

	if (self->_full_cap_wh > 0U) {
		result = (bec_get_remain_cap_kwh(self) /
			  bec_get_full_cap_kwh(self)) * 100.0f;
	}

	return result;
}

void bec_recalc_cap(struct bec *self)
{
	/* Calculate capacity counts */
	int64_t capacity_counts =
		_bec_conv_wh_to_counts(self, self->_full_cap_wh);

	self->_cap_counts += (int64_t)self->_voltage_V *
				(int64_t)self->_current_A;

	/* If voltage is higher than full capacity voltage - increment timer */
	if (self->_voltage_V >= self->_full_cap_voltage_V) {
		self->_full_cap_voltage_debounce_ms +=
						     self->_update_interval_ms;
	} else {
		self->_full_cap_voltage_debounce_ms = 0U;
	}

	/* If voltage was higher for 5 seconds - set capacity too 100% */
	if (self->_full_cap_voltage_debounce_ms >= 5000U) {
		self->_full_cap_voltage_debounce_ms = 0U;
		self->_cap_counts = capacity_counts;
	}

	/* Accumulated energy should not exceed battery capacity 
	 * nor go below negative capacity */
	if (self->_cap_counts > capacity_counts) {
		self->_cap_counts = capacity_counts;
	} else if (self->_cap_counts < 0) {
		self->_cap_counts = 0;
	} else {}
}

void bec_update(struct bec *self, uint32_t delta_time_ms)
{

	self->_update_timer_ms += (int32_t)delta_time_ms;

	if (self->_update_timer_ms >= (int32_t)self->_update_interval_ms) {
		self->_update_timer_ms -= (int32_t)self->_update_interval_ms;

		bec_recalc_cap(self);
	}
}

#endif /* ENERGY_COUNTER_GUARD */
