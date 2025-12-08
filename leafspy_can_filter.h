/******************************************************************************
 * LEAFSPY uses ISO-TP (ISO-15765-2) (ISO 14229-2) protocol for communication
 * We don't implement the protocol, just minimal functional set
 * for our purposes
 *****************************************************************************/

#include <stdint.h>
#include <string.h>

enum leafspy_query_type {
	LEAFSPY_QUERY_TYPE_UNKNOWN,
	LEAFSPY_QUERY_TYPE_LBC_BLOCK1
};

/** Leafspy CAN filter lbc group 1 messages */
struct leafspy_can_lbc {
	float soh; /* State of health */
	float soc; /* State of charge */
	float ah;  /* Amper Hours */
};

/** Leafspy CAN filter. Intercepts leafspy queries and
 *  replaces answers with our custom data */
struct leafspy_can_filter {
	struct leafspy_can_lbc lbc;

	uint8_t _query_type;
	uint8_t _query_num;

	/* Intermediate buffer to work with multiple frames at once */
	struct leaf_can_filter_frame _fbuf[2];

	/* buffer for ISO-TP frames payload */
	uint8_t  _tpbuf[0xFFu];
	uint16_t _tplen;
	uint16_t _tpdlc;
};

void leafspy_can_filter_init(struct leafspy_can_filter *self)
{
	self->lbc.soh = 0.0f;
	self->lbc.soc = 0.0f;
	self->lbc.ah  = 0.0f;

	self->_query_type = LEAFSPY_QUERY_TYPE_UNKNOWN;
	self->_query_num  = 0u;
}

void leafspy_can_filter_process_lbc_block1_answer_frame(
					struct leafspy_can_filter *self,
					struct leaf_can_filter_frame *f)
{
	if (f->data[0] == 0x24u) {
		self->lbc.soh = ((f->data[4] << 8) | f->data[5]) / 102.4f;
	}

	if (f->data[0] == 0x24u) {
		self->_fbuf[0] = *f;
	}

	if (f->data[0] == 0x25u) {
		self->lbc.soc = (self->_fbuf[0].data[7] << 16u |
			((f->data[1] << 8u) | f->data[2])) / 10000.0f;

		self->lbc.ah  = (f->data[4] << 16u |
			((f->data[5] << 8u) | f->data[6])) / 10000.0f;
	}
}

void leafspy_can_filter_dispatch_query_type(struct leafspy_can_filter *self,
					    struct leaf_can_filter_frame *f)
{
	switch (self->_query_type) {
	case LEAFSPY_QUERY_TYPE_LBC_BLOCK1:
		if (f->id != 0x7BBu) {
			break;
		}

		/* First frame */
		if ((f->data[0] & 0xF0u) == 0x10) {
			self->_tpdlc  = (f->data[0] & 0x0Fu) << 8;
			self->_tpdlc |=  f->data[1];

			/* Store first 6byte payload */
			memcpy(self->_tpbuf, &f->data[2], 6u);
			self->_tplen = 6u;			
		} else if ((f->data[0] & 0xF0u) == 0x20u) {
			uint8_t segment_len = (self->_tpdlc - self->_tplen);
			segment_len = (segment_len > 7u) ? 7u : segment_len;

			/* We do not really care about sequence number here */
			/* (f->data[0] & 0x0Fu) */
			
			memcpy(&self->_tpbuf[self->_tplen], &f->data[1],
			       segment_len);

			self->_tplen += segment_len;
		}

		/* DLC can not be bigger than buf size */
		if (self->_tpdlc > 0xFFu || self->_tplen >= self->_tpdlc) {
			self->_query_type = LEAFSPY_QUERY_TYPE_UNKNOWN;
		}

		leafspy_can_filter_process_lbc_block1_answer_frame(self, f);
		break;
	default:
		break;
	}
}

void leafspy_can_filter_process_frame(struct leafspy_can_filter *self,
				      struct leaf_can_filter_frame *f)
{
	switch (f->id) {
	/* LBC query */
	case 0x79Bu: {
		if (f->len != 8u) {
			break;
		}

		/* LeafSpy sends (single frame) request to the vehicle */
		if ((self->_query_num == 0u) &&
		   /* (SF) SingleFrame (7 - 4) == 0x00 */
		   ((f->data[0] & 0xF0) == 0x00u) &&

		   /* (SF_DL) DLC (3 - 0) == ?? */
		   ((f->data[0] & 0x0F) == 0x02u) && /* Expect 2 bytes */

		   /* (SF_DL) DLC (3 - 0) == ?? */
		   /* UDS Service request (0x21)
		    * Leaf uses proprietery UDS service,
		    * which is similar to standard UDS service 0x22 */
		   (f->data[1] == 0x21u) &&

		   /* Request LBC data */
   		   (f->data[2] == 0x01u)
		) {
			self->_tpdlc = 0u;
			self->_tplen = 0u;

			self->_query_type = LEAFSPY_QUERY_TYPE_LBC_BLOCK1;
			self->_query_num = 1u;
		} else if (
		  (self->_query_num == 1u) &&
		   /* (FC) flow control (7 - 4) == 0x30 */
		   ((f->data[0] & 0xF0) == 0x30u)

		   /* (FS) Flow status  (3 - 0) */
		   /* ((f->data[0] & 0x0F) == ??) */

		/* f->data[1] == 0x00u */ /* (BS) BlockSize */

		/* (STmin) SeparationTime minimum (0x0Au == 10ms) */
		/* f->data[2] == 0x0Au */
		) {
			self->_query_type = LEAFSPY_QUERY_TYPE_LBC_BLOCK1;
			self->_query_num  = 0u;
		} else {
			self->_query_type = LEAFSPY_QUERY_TYPE_UNKNOWN;
			self->_query_num  = 0u;
		}
	
		break;
	}

	default:
		break;
	}

	leafspy_can_filter_dispatch_query_type(self, f);
}
