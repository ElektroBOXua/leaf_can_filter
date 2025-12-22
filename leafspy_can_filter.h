/******************************************************************************
 * LEAFSPY uses ISO-TP (ISO-15765-2) (ISO 14229-2) protocol for communication
 * We don't implement the protocol, just minimal functional set
 * for our purposes
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <string.h>
#include "iso_tp.h"

enum leafspy_query_type {
	LEAFSPY_QUERY_TYPE_UNKNOWN,
	LEAFSPY_QUERY_TYPE_LBC_BLOCK1
};

/** Leafspy CAN filter lbc group 1 messages */
struct leafspy_can_lbc_override {
	float ah;
};

struct leafspy_can_lbc {
	float current0_A;
	float current1_A;

	float voltage_V;

	float hx;

	float soc;
	float ah;

	struct leafspy_can_lbc_override ovd;
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

	/* Experimental, test purposes only (replaces LBC01 byte by idx)*/
	uint8_t filter_leafspy_idx;
	uint8_t filter_leafspy_byte;
};

void leafspy_can_filter_init(struct leafspy_can_filter *self)
{
	struct iso_tp_config cfg;

	self->lbc.current0_A = 0.0f;
	self->lbc.current1_A = 0.0f;
	self->lbc.voltage_V  = 0.0f;
	self->lbc.hx  = 0.0f;
	self->lbc.soc = 0.0f;
	self->lbc.ah  = 0.0f;
	self->lbc.ovd.ah = 0.0f;

	iso_tp_init(&self->iso_tp);

	/* Get current configuration */
	iso_tp_get_config(&self->iso_tp, &cfg);

	/* Set new configuration */
	cfg.tx_dl = 8u; /* CAN2.0 */
	iso_tp_set_config(&self->iso_tp, &cfg);

	self->_state = 0u;

	self->_full_sn  = 0u;
	self->_len_buf = 0u;

	/* Experimental, test purposes only (replaces LBC01 byte by idx)*/
	self->filter_leafspy_idx  = 0u;
	self->filter_leafspy_byte = 0u;
}

void leafspy_can_filter_process_lbc_block1_answer_pdu(
					struct leafspy_can_filter *self,
					struct iso_tp_n_pdu *n_pdu)
{
	uint8_t *d = n_pdu->n_data;

	/* Don't put that anywhere near sane code...
	 * Replaces a single byte inside lbc01 only for testing purposes */
	if (self->filter_leafspy_idx < 2u) {
		/* We do not replace less than 2 bytes */
	} else if ((self->_len_buf == 0u) && (self->filter_leafspy_idx < 6u)) {
		d[self->filter_leafspy_idx] = self->filter_leafspy_byte;
		(void)iso_tp_override_n_pdu(&self->iso_tp, n_pdu);
	} else if ((self->filter_leafspy_idx >= self->_len_buf) &&
		   ((self->filter_leafspy_idx - self->_len_buf) < 7)) {
		d[self->filter_leafspy_idx - self->_len_buf] =
			self->filter_leafspy_byte;
		(void)iso_tp_override_n_pdu(&self->iso_tp, n_pdu);
	} else {}

	switch (self->_full_sn) {
	case 0u: { /* 0 ... 5 */
		int32_t current_raw = 0u;

		current_raw |= (d[2] << 24u);
		current_raw |= (d[3] << 16u);
		current_raw |= (d[4] << 8u);
		current_raw |= (d[5] << 0u);

		self->lbc.current0_A = (float)current_raw / 1000.0f;

		break;
	}

	case 1u: { /* 6 ... 12 */
		int32_t current_raw = 0u;

		current_raw |= (d[2] << 24u);
		current_raw |= (d[3] << 16u);
		current_raw |= (d[4] << 8u);
		current_raw |= (d[5] << 0u);

		self->lbc.current1_A = (float)current_raw / 1000.0f;

		break;
	}

	case 2u: /* 13 ... 19 */
		break;

	case 3u: /* 20 ... 26 */
		self->lbc.voltage_V = ((d[0] << 8) | d[1]) / 100.0f;
		break;
	case 4u: /* 27 ... 33 */
		self->lbc.hx  = ((d[1] << 8) | d[2]) / 100.0f;
		self->lbc.soc = ((d[4] << 16u) | (d[5] << 8u) | d[6]) /
				10000.0f;
		break;
	case 5u: /* 34 ... 40 */
		self->lbc.ah = ((d[1] << 8) | d[2]) / 39.0f; /* Weird */

		/* If AmpHours override enabled */
		if (self->lbc.ovd.ah > 0.0f) {
			uint16_t ah_raw = (self->lbc.ovd.ah * 39.0f);
			d[1] = (ah_raw & 0xFF00u) >> 8u;
			d[2] = (ah_raw & 0x00FFu) >> 0u;
			(void)iso_tp_override_n_pdu(&self->iso_tp, n_pdu);
		}

		break;

	default:
		break;
	}

	/* TODO make dlc exact ff_dl */
	if ((self->_len_buf + n_pdu->len_n_data) < 0xFFu) {
		(void)memcpy(&self->_buf[self->_len_buf],
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
	(void)memcpy(&f.data, _f->data, _f->len);

	if ((f.id == obd_id) || (f.id == lbc_id)) {
		(void)iso_tp_push_frame(&self->iso_tp, &f);
	}

	(void)iso_tp_step(&self->iso_tp, 0u);
	has_n_pdu = iso_tp_get_n_pdu(&self->iso_tp, &n_pdu);

	/* Reset on error */
	if (iso_tp_has_cf_err(&self->iso_tp)) {
		self->_state   = 0u;
	}

	switch (self->_state) {
	case 0u: /* Initial state, observe request */
		if (has_n_pdu && (f.id == obd_id) &&
		    (n_pdu.n_pci.n_pcitype == (uint8_t)ISO_TP_N_PCITYPE_SF) &&
		    (n_pdu.n_pci.sf_dl == 2u) && (n_pdu.len_n_data == 2u) &&
		    ((n_pdu.n_data[0] == 0x21u) &&
		     (n_pdu.n_data[1] == 0x01u))) {
			self->_full_sn = 0u;
			self->_state   = 1u;
		}

		break;

	case 1u: /* Initial state, observe first frame */
		if (has_n_pdu && (f.id == lbc_id) &&
		    (n_pdu.n_pci.n_pcitype == (uint8_t)ISO_TP_N_PCITYPE_FF) &&
		    (n_pdu.len_n_data == 6u) &&
		    /* Only certain ff_dl are allowed
		     * (vehicle version specific) TODO */
		     ((n_pdu.n_pci.ff_dl == 41u) ||
		      (n_pdu.n_pci.ff_dl == 43u))) {
			self->_len_buf = 0u;
			leafspy_can_filter_process_lbc_block1_answer_pdu(self,
								       &n_pdu);
			self->_full_sn++;
			self->_state  = 2u;
		}

		break;

	case 2u: /* Observe consecutive frame */
		if (has_n_pdu &&
		    (n_pdu.n_pci.n_pcitype == (uint8_t)ISO_TP_N_PCITYPE_CF) &&
		    (n_pdu.len_n_data == 7u)) {
			leafspy_can_filter_process_lbc_block1_answer_pdu(self,
								       &n_pdu);
			self->_full_sn++;

			if (self->_len_buf >= n_pdu.n_pci.ff_dl) {
				self->_state = 0u;
			}
		}
		
		break;

	default:
		self->_state = 0u;
		break;
	}

	/* If frame is overriden */
	if (iso_tp_pop_frame(&self->iso_tp, &f)) {
		/* _f->id  = f.id;
		   _f->len = f.len; Do not override original frame len/id?*/
		(void)memcpy(_f->data, &f.data, _f->len);
	}
}
