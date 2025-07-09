#ifndef   ENERGY_COUNTER_GUARD
#define   ENERGY_COUNTER_GUARD

#include <stdint.h>

/******************************************************************************
 * CLASS
 *****************************************************************************/
struct bec
{
	/* Config */
	float    _battery_capacity_kwh;
	int16_t  _100_pct_voltage_V;
	uint32_t _update_interval_ms;
	
	/* Runtime */
	int16_t _voltage_V;
	int16_t _current_A;

	/* Energy counter (accumulator) */
	int64_t _energy_counts;
	
	int32_t _update_timer_ms;
};

/******************************************************************************
 * PRIVATE
 *****************************************************************************/
int64_t _bec_get_counts_per_hour(struct bec *self)
{
	return ((1000U / self->_update_interval_ms) * 60U * 60U);
}

int64_t _bec_conv_kwh_to_counts(struct bec *self, float val)
{
	return (int64_t)(val * 1000.f) * _bec_get_counts_per_hour(self);
}

/******************************************************************************
 * INIT
 *****************************************************************************/
void bec_init(struct bec *self)
{
	/* Config */
	self->_battery_capacity_kwh = 0U;
	self->_100_pct_voltage_V    = 0U;
	self->_update_interval_ms   = 0U;

	/* Runtime */
	self->_voltage_V          = 0U;
	self->_current_A          = 0U;
	self->_energy_counts = 0;
	
	self->_update_timer_ms = 0;
}

/******************************************************************************
 * CONFIG
 *****************************************************************************/
void bec_set_battery_capacity_kwh(struct bec *self, float val)
{
	self->_battery_capacity_kwh = val;
}

float bec_get_battery_capacity_kwh(struct bec *self)
{
	return self->_battery_capacity_kwh;
}

void bec_set_initial_energy_kwh(struct bec *self, float val)
{
	int64_t e = (int64_t)(val * 1000.0f); /* convert to watts */

	/* Setting initial energy without update interval
	 * is undefined behaviour (TODO define) */
	assert(self->_update_interval_ms != 0U);

	e = e * _bec_get_counts_per_hour(self);

	self->_energy_counts = e;
}

void bec_set_100_pct_voltage_V(struct bec *self, int16_t val)
{
	self->_100_pct_voltage_V = val;
}

void bec_set_update_interval_ms(struct bec *self, uint32_t val)
{
	/* Changing interval is undefined behaviour (TODO define) */
	assert(self->_energy_counts == 0);

	self->_update_interval_ms = val;
	self->_update_timer_ms    = val;
}


/******************************************************************************
 * RUNTIME
 *****************************************************************************/
/* 1V/bit precision */
void bec_set_voltage_V(struct bec *self, int16_t val)
{
	self->_voltage_V = val;
}

/* 1A/bit precision */
void bec_set_current_A(struct bec *self, int16_t val)
{
	self->_current_A = val;
}

/* 1W/bit precision */
float bec_get_energy_kwh(struct bec *self)
{
	/* Divide accumulated energy to update intervals per hour,
	 * then convert watts to kilowatts (divide by 1000) */
	return (self->_energy_counts / _bec_get_counts_per_hour(self)) /
		1000.0f;
}

float bec_get_soc_pct(struct bec *self)
{
	float result = 0.0f;

	if (self->_battery_capacity_kwh > 0.0f) {
		result = (bec_get_energy_kwh(self) /
			  self->_battery_capacity_kwh) * 100.0f;
	}

	return result;
}

void bec_recalc_energy(struct bec *self)
{
	/* Calculate capacity counts */
	int64_t capacity_counts =
		_bec_conv_kwh_to_counts(self, self->_battery_capacity_kwh);

	self->_energy_counts += (int64_t)self->_voltage_V *
				(int64_t)self->_current_A;

	if (self->_voltage_V >= self->_100_pct_voltage_V) {
		self->_energy_counts = capacity_counts;
	}

	/* Accumulated energy should not exceed battery capacity 
	 * nor go below negative capacity */
	if (self->_energy_counts > capacity_counts) {
		self->_energy_counts = capacity_counts;
	} else if (self->_energy_counts < 0) {
		self->_energy_counts = 0;
	} else {}
}

void bec_update(struct bec *self, uint32_t delta_time_ms)
{

	self->_update_timer_ms += (int32_t)delta_time_ms;

	if (self->_update_timer_ms >= (int32_t)self->_update_interval_ms) {
		self->_update_timer_ms -= (int32_t)self->_update_interval_ms;

		bec_recalc_energy(self);
	}
}

#endif /* ENERGY_COUNTER_GUARD */
