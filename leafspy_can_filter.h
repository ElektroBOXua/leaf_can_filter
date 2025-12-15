/******************************************************************************
 * LEAFSPY uses ISO-TP (ISO-15765-2) (ISO 14229-2) protocol for communication
 * We don't implement the protocol, just minimal functional set
 * for our purposes
 *****************************************************************************/

#include <stdint.h>
#include <string.h>
#include "iso_tp.h"

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

	struct iso_tp iso_tp;

	uint8_t _state;

	uint32_t _full_sn;
	uint8_t  _buf[0xFF];
	uint8_t  _len_buf;
};

void leafspy_can_filter_init(struct leafspy_can_filter *self)
{
	struct iso_tp_config cfg;

	self->lbc.soh = 0.0f;
	self->lbc.soc = 0.0f;
	self->lbc.ah  = 0.0f;

	iso_tp_init(&self->iso_tp);

	/* Get current configuration */
	iso_tp_get_config(&self->iso_tp, &cfg);

	/* Set new configuration */
	cfg.tx_dl = 8u; /* CAN2.0 */
	iso_tp_set_config(&self->iso_tp, &cfg);

	self->_state = 0u;

	self->_full_sn  = 0u;
	self->_len_buf = 0u;
}

void leafspy_can_filter_process_lbc_block1_answer_pdu(
					struct leafspy_can_filter *self,
					struct iso_tp_n_pdu *n_pdu)
{
	uint8_t *d = n_pdu->n_data;

	switch (self->_full_sn) {
	case 0u:

		d[2] = 0xFF;
		d[3] = 0xFF;
		d[4] = 0xFF;
		d[5] = 0xDF;

		iso_tp_override_n_pdu(&self->iso_tp, n_pdu);

		break;

	case 1u:
		d[2] = 0xFF;
		d[3] = 0xFF;
		d[4] = 0xFF;
		d[5] = 0xDF;

		iso_tp_override_n_pdu(&self->iso_tp, n_pdu);

		break;

	case 4u:
		self->lbc.soh = ((d[4] << 8) | d[5]) / 102.4f;
		self->lbc.soc = (d[7] << 16u);

		break;

	case 5u:
		self->lbc.soc += ((d[1] << 8u) | d[2]) / 10000.0f;

		self->lbc.ah   = (d[4] << 16u | ((d[5] << 8u) | d[6])) /
				  10000.0f;

		break;
	}

	if ((self->_len_buf + n_pdu->len_n_data) < 0xFFu) {
		memcpy(&self->_buf[self->_len_buf],
			n_pdu->n_data, n_pdu->len_n_data);
		self->_len_buf += n_pdu->len_n_data;
	}
}

/* This is so shit... */
void leafspy_can_filter_process_lbc_block1_frame(
					struct leafspy_can_filter *self,
				        struct leaf_can_filter_frame *_f)
{
	struct iso_tp_can_frame f;

	/* Prepare stuff to observe */
	struct iso_tp_n_pdu n_pdu;
	const uint32_t obd_id  = 0x0000079Bu;
	const uint32_t lbc_id  = 0x000007BBu;
	bool has_n_pdu = false;

	/* Copy example frame from log*/
	f.id   = _f->id;
	f.len  = _f->len;
	memcpy(&f.data, _f->data, _f->len);

	if ((f.id == obd_id) || (f.id == lbc_id)) {
		iso_tp_push_frame(&self->iso_tp, &f);
	}

	iso_tp_step(&self->iso_tp, 0u);
	has_n_pdu = iso_tp_get_n_pdu(&self->iso_tp, &n_pdu);

	/* Reset on error */
	if (iso_tp_has_cf_err(&self->iso_tp)) {
		self->_state   = 0u;
	}

	switch (self->_state) {
	case 0u: /* Initial state, observe request */
		if (has_n_pdu && (f.id == obd_id) &&
		    (n_pdu.n_pci.n_pcitype == ISO_TP_N_PCITYPE_SF) &&
		    (n_pdu.n_pci.sf_dl == 2u) && (n_pdu.len_n_data == 2u) &&
		    (n_pdu.n_data[0] == 0x21u && n_pdu.n_data[1] == 0x01u)) {
			self->_full_sn = 0u;
			self->_state   = 1u;
		}

		break;

	case 1u: /* Initial state, observe first frame */
		if (has_n_pdu && (f.id == lbc_id) &&
		    (n_pdu.n_pci.n_pcitype == ISO_TP_N_PCITYPE_FF) &&
		    (n_pdu.len_n_data == 6u)) {
			self->_len_buf = 0u;
			leafspy_can_filter_process_lbc_block1_answer_pdu(self,
								       &n_pdu);
			self->_full_sn++;
			self->_state  = 2u;
		}

		break;

	case 2u: /* Observe consecutive frame */
		if (has_n_pdu &&
		    (n_pdu.n_pci.n_pcitype == ISO_TP_N_PCITYPE_CF) &&
		    (n_pdu.len_n_data == 7u)) {
			leafspy_can_filter_process_lbc_block1_answer_pdu(self,
								       &n_pdu);
			self->_full_sn++;

			if (self->_len_buf >= n_pdu.n_pci.ff_dl) {
				self->_state = 0u;
			}
		}
		
		break;
	}

	/* If frame is overriden */
	if (iso_tp_pop_frame(&self->iso_tp, &f)) {
		/* _f->id  = f.id;
		   _f->len = f.len; Do not override original frame len/id?*/
		memcpy(_f->data, &f.data, _f->len);
	}
}
