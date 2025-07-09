#ifndef   LEAF_CAN_FILTER_GUARD
#define   LEAF_CAN_FILTER_GUARD

#ifdef    LEAF_CAN_FILTER_DEBUG
#define LEAF_CAN_FILTER_LOG_U16(v) ((void)printf("%s: %u\n", #v, v))
#define LEAF_CAN_FILTER_LOG_I16(v) ((void)printf("%s: %i\n", #v, v))
#else
#define LEAF_CAN_FILTER_LOG_U16(v)
#define LEAF_CAN_FILTER_LOG_I16(v)
#endif /* LEAF_CAN_FILTER_DEBUG */

#include <stdint.h>
#include "bite.h"
#include "energy_counter.h"

/******************************************************************************
 * CLASS
 *****************************************************************************/
struct leaf_bms_vars {
	uint16_t voltage_V;
	int16_t  current_A;
	
	uint32_t remain_capacity_wh;
	uint32_t full_capacity_wh;
};

void leaf_bms_vars_init(struct leaf_bms_vars *self)
{
	self->voltage_V = 0U;
	self->current_A = 0;

	self->remain_capacity_wh = 0U;
	self->full_capacity_wh   = 0U;
}

/******************************************************************************
 * CLASS
 *****************************************************************************/
/* Leaf can frame data structure */
struct leaf_can_filter_frame {
	uint32_t id;
	uint8_t  len;
	uint8_t  data[8];
};

struct leaf_can_filter_settings {
	/* Bypass filtering completely */
	bool bypass;

	/* Override bms capacity (enables energy counter) */
	bool  capacity_override_enabled;
	float capacity_override_kwh;
};

struct leaf_can_filter {
	struct bite _b;   /* bitstream manipulator */
	struct bec  _bec; /* Energy counter */

	struct leaf_can_filter_settings settings;
	struct leaf_bms_vars _bms_vars;
};

/******************************************************************************
 * PRIVATE
 *****************************************************************************/
/* Filter HVBAT frames */
void _leaf_can_filter(struct leaf_can_filter *self,
		      struct leaf_can_filter_frame *frame)
{
	bite_set_buf(&self->_b, frame->data, frame->len);

	switch (frame->id) {
	/* BO_ 1468 x5BC: 8 HVBAT */
	case 1468U: {
		uint16_t remain_capacity_gids = 0U;
		uint16_t full_capacity        = 0U;

		/* SG_ LB_Remain_Capacity :
		 * 	7|10@0+ (1,0) [0|500] "gids" Vector__XXX */
		bite_begin(&self->_b, 7U, 10U, BITE_ORDER_DBC_0);

		remain_capacity_gids = bite_read_u16(&self->_b);

		if (self->settings.capacity_override_enabled) {
			uint16_t overriden =
			  bec_get_remain_cap_kwh(&self->_bec) * 10.0;
			LEAF_CAN_FILTER_LOG_U16(overriden);  /* TODO FIX LIB */
			
			bite_rewind(&self->_b);
			bite_write_16(&self->_b, overriden);
		}

		bite_end(&self->_b);


		/* SG_ LB_New_Full_Capacity :
		 * 	13|10@0+ (80,250) [20000|24000] "wh" Vector__XXX */
		bite_begin(&self->_b, 13U, 10U, BITE_ORDER_DBC_0);
		full_capacity = bite_read_u16(&self->_b);

		if (self->settings.capacity_override_enabled) {
			uint16_t overriden = 
				bec_get_full_cap_kwh(&self->_bec) * 10.0;

			LEAF_CAN_FILTER_LOG_U16(overriden);

			bite_rewind(&self->_b);
			bite_write_16(&self->_b, overriden); /* TODO FIX LIB */
		}

		bite_end(&self->_b);

		/* SG_ LB_Remaining_Capacity_Segment m1 :
		 * 	16|4@1+ (1,0) [0|12] "dash bars" Vector__XXX */

		/* bite_begin(&self->_b, 16, 4, BITE_ORDER_DBC_1);
		   bite_write(&self->_b, 12);
		   bite_end(&self->_b);
		   */			

		self->_bms_vars.remain_capacity_wh  = remain_capacity_gids;
		self->_bms_vars.remain_capacity_wh *= 80U;
		
		self->_bms_vars.full_capacity_wh   = full_capacity;
		self->_bms_vars.full_capacity_wh  *= 80U;
		self->_bms_vars.full_capacity_wh  += 250U;

		LEAF_CAN_FILTER_LOG_U16(remain_capacity_gids);
		LEAF_CAN_FILTER_LOG_U16(full_capacity);

		LEAF_CAN_FILTER_LOG_U16(self->_bms_vars.remain_capacity_wh);
		LEAF_CAN_FILTER_LOG_U16(self->_bms_vars.full_capacity_wh);

		break;
	}

	/* BO_ 475 x1DB: 8 HVBAT */
	case 475U: {
		uint16_t voltage_V = 0U;
		int16_t  current_A = 0;

		/* SG_ LB_Total_Voltage :
		 * 	23|10@0+ (0.5,0) [0|450] "V" Vector__XXX */
		bite_begin(&self->_b, 23U, 10U, BITE_ORDER_DBC_0);
		voltage_V = bite_read_u16(&self->_b);
		bite_end(&self->_b);

		/* There's an error in dbc file! Value is signed actually. */
		/* SG_ LB_Current :
		 * 	7|11@0+ (0.5,0) [-400|200] "A" Vector__XXX */
		bite_begin(&self->_b, 7U, 11U, BITE_ORDER_DBC_0);
		current_A = bite_read_i16(&self->_b);
		bite_end(&self->_b);

		self->_bms_vars.voltage_V = voltage_V;
		self->_bms_vars.current_A = current_A;

		/* Report voltage and current to our energy counter 
		 * TODO (stop dividing by 2, bec must accept scaled values) */
		bec_set_voltage_V(&self->_bec, (uint16_t)voltage_V / 2U);
		bec_set_current_A(&self->_bec, (uint16_t)current_A / 2U);

		LEAF_CAN_FILTER_LOG_U16(voltage_V);
		LEAF_CAN_FILTER_LOG_I16(current_A);

		break;
	}
	
	default:
		break;
	}
}

void _leaf_can_filter_update(struct leaf_can_filter *self,
			     uint32_t delta_time_ms)
{
	bec_update(&self->_bec, delta_time_ms);
}

/******************************************************************************
 * PUBLIC
 *****************************************************************************/
void leaf_can_filter_init(struct leaf_can_filter *self)
{
	struct leaf_can_filter_settings *s = &self->settings;

	bite_init(&self->_b);

	/* Settings (DEFAULT) */
	s->bypass = true;

	s->capacity_override_enabled = false;
	s->capacity_override_kwh     = 0.0f;

	/* Runtime */
	bec_init(&self->_bec);
	bec_set_update_interval_ms(&self->_bec, 10U);
	leaf_bms_vars_init(&self->_bms_vars);

	/* ... */
}

void leaf_can_filter_process_frame(struct leaf_can_filter *self,
				   struct leaf_can_filter_frame *frame)
{
	/* Only process frame when bypass is disabled. */
	if (self->settings.bypass == false) {
		_leaf_can_filter(self, frame);
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
