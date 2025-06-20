#ifndef   LEAF_CAN_FILTER_GUARD
#define   LEAF_CAN_FILTER_GUARD
#include <stdint.h>
#include "bite.h"

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
	uint8_t dummy;
};

struct leaf_can_filter_runtime {
	uint8_t dummy;
};

struct leaf_can_filter {
	struct bite _b;

	struct leaf_can_filter_settings settings;
	struct leaf_can_filter_runtime  runtime;
};

/******************************************************************************
 * PRIVATE
 *****************************************************************************/
/* Filter HVBAT frames */
void _leaf_can_filter(struct leaf_can_filter *self,
		      struct leaf_can_filter_frame *frame)
{
	bite_init(&self->_b, frame->data, frame->len);

	switch (frame->id) {
	/* BO_ 1468 x5BC: 8 HVBAT */
	case 1468U:
		/* SG_ LB_Remaining_Capacity_Segment m1 :
		 * 	16|4@1+ (1,0) [0|12] "dash bars" Vector__XXX */

		bite_begin(&self->_b, 16, 4, BITE_ORDER_DBC_1);
		bite_write(&self->_b, 12);
		bite_end(&self->_b);
		break;
	
	default:
		break;
	}
}

/******************************************************************************
 * PUBLIC
 *****************************************************************************/
void leaf_can_filter_init(struct leaf_can_filter *self)
{
	struct leaf_can_filter_settings *s = &self->settings;
	struct leaf_can_filter_runtime  *r = &self->runtime;

	/* Just a dummy variable (TODO REMOVE) */
	s->dummy = 0;
	r->dummy = 0;

	/* ... */
}

void leaf_can_filter_process_frame(struct leaf_can_filter *self,
				   struct leaf_can_filter_frame *frame)
{
	_leaf_can_filter(self, frame);
}

void leaf_can_filter_update(struct leaf_can_filter *self)
{
	(void)self;
}

#endif /* LEAF_CAN_FILTER_GUARD */
