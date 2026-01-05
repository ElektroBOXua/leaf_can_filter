#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "lcf_keygen.h"

/* ISO-TP leaf can filter SOH reset hardcoded emulation */
#define LCF_SR_HEARTBEAT_RATE_MS 2000u
#define LCF_SR_TX_ID 0x79Bu
#define LCF_SR_RX_ID 0x7BBu
#define LCF_SR_RX_TIMEOUT_MS 2000u

enum lcf_sr_status {
	LCF_SR_STATUS_STOPPED,
	LCF_SR_STATUS_ACTIVE,
	LCF_SR_STATUS_SUCCEED,
	LCF_SR_STATUS_FAILED,
	LCF_SR_STATUS_TIMEOUT
};

enum lcf_sr_state {
	LCF_SR_STATE_STOPPED,
	LCF_SR_STATE_IDLE,

	/*LCF_SR_STATE_UDS_SESSION_START
	LCF_SR_STATE_UDS_SESSION_START_RESPONSE*/

	/* Privileged session */
	LCF_SR_STATE_UDS_SESSION_PRIVIL,
	LCF_SR_STATE_UDS_SESSION_PRIVIL_RESPONSE,

	/* Request access to secured functions */
	LCF_SR_STATE_UDS_REQUEST_SECURITY,
	LCF_SR_STATE_UDS_REQUEST_SECURITY_RESPONSE,
	LCF_SR_STATE_UDS_SEND_SOLVED_CHALLENGE,
	LCF_SR_STATE_UDS_SEND_SOLVED_CHALLENGE_RESPONSE,

	/* Heartbeat */
	LCF_SR_STATE_UDS_TESTER_PRESENT,
	LCF_SR_STATE_UDS_TESTER_PRESENT_RESPONSE,

	/* Call services */
	LCF_SR_STATE_UDS_CALL_SERVICE0,
	LCF_SR_STATE_UDS_CALL_SERVICE0_RESPONSE,
	LCF_SR_STATE_UDS_CALL_SERVICE1,
	LCF_SR_STATE_UDS_CALL_SERVICE1_RESPONSE,

	/* Default session */
	LCF_SR_STATE_UDS_SESSION_DEFAULT,
	LCF_SR_STATE_UDS_SESSION_DEFAULT_RESPONSE
};

struct lcf_sr {
	uint8_t _state;
	uint8_t _status;

	uint32_t _heartbeat_clock_ms;

	struct leaf_can_filter_frame _tx;
	bool _has_tx;
	struct leaf_can_filter_frame _rx;
	bool _has_rx;

	uint32_t _timeout_ms;
	uint32_t _timer_ms;

	uint32_t _incoming_challenge;
	uint8_t  _solved_challenge[8];
	uint8_t  _sn;
};

void lcf_sr_init(struct lcf_sr *self)
{
	self->_state = 0u;
	self->_status = LCF_SR_STATUS_STOPPED;

	self->_heartbeat_clock_ms =  LCF_SR_HEARTBEAT_RATE_MS;

	self->_has_tx = false;
	self->_has_rx = false;

	self->_timeout_ms = 0u;
	self->_timer_ms   = 0u;
}

void _lcf_sr_push_tx(struct lcf_sr *self, uint8_t *data, uint8_t len)
{
		/* Prepare TX frame for popping */
		self->_has_tx = true;
		self->_tx.id  = LCF_SR_TX_ID;
		self->_tx.len = len;
		(void)memcpy(self->_tx.data, data, 8u);
}

/** Push RX CAN frame for processing, returns false if busy. */
bool lcf_sr_push_frame(struct lcf_sr *self, struct leaf_can_filter_frame *f)
{
	bool result = false;

	if (!self->_has_rx && (f->id == LCF_SR_RX_ID)) {
		self->_has_rx = true;

		self->_rx = *f;

		result = true;
	}

	return result;
}

/** Pop TX CAN frame, returns false if busy.
 * TODO not yet implemented */
bool lcf_sr_pop_frame(struct lcf_sr *self, struct leaf_can_filter_frame *f)
{
	bool result = false;

	if (self->_has_tx) {
		self->_has_tx = false;

		if (f != NULL) {
			*f = self->_tx;
		}

		result = true;
	}

	return result;
}

void lcf_sr_start(struct lcf_sr *self)
{
	lcf_sr_init(self);

	self->_state = LCF_SR_STATE_IDLE;
	self->_status = LCF_SR_STATUS_ACTIVE;
}

void lcf_sr_stop(struct lcf_sr *self)
{
	lcf_sr_init(self);

	self->_status = LCF_SR_STATUS_STOPPED;
}

void lcf_sr_abort(struct lcf_sr *self, uint8_t status)
{
	lcf_sr_init(self);
	self->_status = status;
}

uint8_t lcf_sr_get_status(struct lcf_sr *self)
{
	return self->_status;
}

/* Prevalidate (validate everything ecept payload) */
bool _lcf_sr_pre_validate_response(struct lcf_sr *self, uint8_t *expec,
				   size_t len)
{
	bool result = false;

	/* Abort if no response */
	if (self->_timeout_ms >= LCF_SR_RX_TIMEOUT_MS) {
		lcf_sr_abort(self, LCF_SR_STATUS_TIMEOUT);

	/* Ignore if no rx */
	} else if (!self->_has_rx) {

	/* Ignore if invalid message */
	} else if ((self->_rx.len < len) || (self->_rx.data[0] != expec[0])) {

	/* Everything is fine in any other case */
	} else {
		result = true;
	}

	self->_has_rx = false;

	return result;
}

/* validate (validate everything + payload) */
bool _lcf_sr_validate_response(struct lcf_sr *self, uint8_t *expec, size_t len)
{
	bool result = false;

	if (_lcf_sr_pre_validate_response(self, expec, len))
	{
		if (memcmp(self->_rx.data, expec, len) != 0) {
			lcf_sr_abort(self, LCF_SR_STATUS_FAILED);
		} else {
			result = true;
		}
	}

	return result;
}

void _lcf_sr_step(struct lcf_sr *self, uint32_t delta_time_ms)
{
	uint8_t state = self->_state;

	self->_heartbeat_clock_ms += delta_time_ms;

	/* Switch to heartbeat if timeout and not doing anything */
	if (self->_heartbeat_clock_ms >= LCF_SR_HEARTBEAT_RATE_MS) {
		self->_heartbeat_clock_ms -= LCF_SR_HEARTBEAT_RATE_MS;

		if (self->_state == (uint8_t)LCF_SR_STATE_IDLE) {
			state = LCF_SR_STATE_UDS_TESTER_PRESENT;
		}
	}

	self->_timer_ms += delta_time_ms;

	switch (state) {
	case LCF_SR_STATE_IDLE:
		if (self->_timer_ms >= (LCF_SR_HEARTBEAT_RATE_MS + 500u)) {
			self->_state = LCF_SR_STATE_UDS_SESSION_PRIVIL;
		}

		break;

	case LCF_SR_STATE_UDS_TESTER_PRESENT: {
		uint8_t data[8] = {0x02u, 0x3Eu, 0x01u, 0xFFu,
				   0xFFu, 0xFFu, 0xFFu, 0xFFu};
		
		_lcf_sr_push_tx(self, data, 8u);

		self->_timeout_ms = 0u;
		self->_state = LCF_SR_STATE_UDS_TESTER_PRESENT_RESPONSE;
		break;
	}

	/* Wait for positive response */
	case LCF_SR_STATE_UDS_TESTER_PRESENT_RESPONSE: {
		uint8_t expec[8] = {0x01u, 0x7Eu, 0xFFu, 0xFFu,
				    0xFFu, 0xFFu, 0xFFu, 0xFFu};

		self->_timeout_ms += delta_time_ms;

		if (!_lcf_sr_validate_response(self, expec, sizeof(expec))) {
			break;
		}

		self->_state = LCF_SR_STATE_IDLE;

		break;
	}

	case LCF_SR_STATE_UDS_SESSION_PRIVIL: {
		uint8_t data[8] = {0x02u, 0x10u, 0xC0u, 0xFFu,
				   0xFFu, 0xFFu, 0xFFu, 0xFFu};

		/* Wait 50ms before service call */
		if (self->_timer_ms < 50u) {
			break;
		}

		_lcf_sr_push_tx(self, data, 8u);

		self->_timeout_ms = 0u;
		self->_state = LCF_SR_STATE_UDS_SESSION_PRIVIL_RESPONSE;
		break;
	}

	case LCF_SR_STATE_UDS_SESSION_PRIVIL_RESPONSE: {
		uint8_t expec[8] = {0x02u, 0x50u, 0xC0u, 0xFFu,
				    0xFFu, 0xFFu, 0xFFu, 0xFFu};

		self->_timeout_ms += delta_time_ms;

		if (!_lcf_sr_validate_response(self, expec, sizeof(expec))) {
			break;
		}

		self->_timer_ms = 0u;
		self->_state    = LCF_SR_STATE_UDS_REQUEST_SECURITY;
		break;
	}

	case LCF_SR_STATE_UDS_REQUEST_SECURITY: {
		uint8_t data[8] = {0x02u, 0x27u, 0x65u, 0xFFu,
				   0xFFu, 0xFFu, 0xFFu, 0xFFu};

		if (self->_timer_ms < 10u) {
			break;
		}

		_lcf_sr_push_tx(self, data, 8u);

		self->_timeout_ms = 0u;
		self->_state = LCF_SR_STATE_UDS_REQUEST_SECURITY_RESPONSE;
		break;
	}

	case LCF_SR_STATE_UDS_REQUEST_SECURITY_RESPONSE: {
		uint8_t expec[8] = {0x06u, 0x67u, 0x65u, 0xFFu,
				    0xFFu, 0xFFu, 0xFFu, 0xFFu};

		self->_timeout_ms += delta_time_ms;

		if (!_lcf_sr_pre_validate_response(self, expec, sizeof(expec)))
		{
			break;
		}

		/* Receive challenge seed */
		self->_incoming_challenge = (self->_rx.data[3] << 24u) |
					    (self->_rx.data[4] << 16u) |
					    (self->_rx.data[5] << 8u)  |
					    (self->_rx.data[6] << 0u);

		decodeChallengeData(self->_incoming_challenge,
				    self->_solved_challenge);

		self->_sn = 0; /* iso-tp consecutive frame sequence num */

		self->_timer_ms = 0u;
		self->_state = LCF_SR_STATE_UDS_SEND_SOLVED_CHALLENGE;
		break;

	}

	case LCF_SR_STATE_UDS_SEND_SOLVED_CHALLENGE: {
		/* Wait ms */
		if (self->_timer_ms < 10u) {
			break;
		}

		if (self->_sn == 0u) {
			uint8_t data[8] = {0x10u, 0x0Au, 0x27u, 0x66u,
					   0xFFu, 0xFFu, 0xFFu, 0xFFu};

			(void)memcpy(&data[4], self->_solved_challenge, 4u);

			_lcf_sr_push_tx(self, data, 8u);

			self->_sn += 1u;

			/* Repeat */
			self->_timer_ms = 0u;
			break;

			/* We don't await flow control response from vehicle
			 * at the moment... TODO */
		} else if (self->_sn == 1u) {
			uint8_t data[8] = {0x21u, 0xFFu, 0xFFu, 0xFFu,
					   0xFFu, 0xFFu, 0xFFu, 0xFFu};

			(void)memcpy(&data[1], &self->_solved_challenge[4u],
				     4u);

			_lcf_sr_push_tx(self, data, 8u);

			self->_sn += 1u;

			/* Discard any RX at this point (FlowControl) */
			self->_has_rx = false;
		} else {}

		self->_timeout_ms = 0u;
		self->_state = LCF_SR_STATE_UDS_SEND_SOLVED_CHALLENGE_RESPONSE;
		break;
	}

	case LCF_SR_STATE_UDS_SEND_SOLVED_CHALLENGE_RESPONSE: {
		uint8_t expec[8] = {0x02u, 0x67u, 0x66u, 0xFFu,
				    0xFFu, 0xFFu, 0xFFu, 0xFFu};

		self->_timeout_ms += delta_time_ms;

		if (!_lcf_sr_validate_response(self, expec, sizeof(expec))) {
			break;
		}

		self->_timer_ms = 0u;
		self->_state    = LCF_SR_STATE_UDS_CALL_SERVICE0;
		break;
	}

	case LCF_SR_STATE_UDS_CALL_SERVICE0: {
		uint8_t data[8] = {0x03u, 0x31u, 0x03u, 0x00u,
				   0xFFu, 0xFFu, 0xFFu, 0xFFu};

		_lcf_sr_push_tx(self, data, 8u);

		self->_timeout_ms = 0u;
		self->_state = LCF_SR_STATE_UDS_CALL_SERVICE0_RESPONSE;
		break;
	}

	case LCF_SR_STATE_UDS_CALL_SERVICE0_RESPONSE: {
		uint8_t expec[8] = {0x03u, 0x71u, 0x03u, 0x01u,
				    0xFFu, 0xFFu, 0xFFu, 0xFFu};

		self->_timeout_ms += delta_time_ms;

		if (!_lcf_sr_validate_response(self, expec, sizeof(expec))) {
			break;
		}

		self->_timer_ms = 0u;
		self->_state = LCF_SR_STATE_UDS_CALL_SERVICE1;

		break;
	}

	case LCF_SR_STATE_UDS_CALL_SERVICE1: {
		uint8_t data[8] = {0x03u, 0x31u, 0x03u, 0x01u,
				   0xFFu, 0xFFu, 0xFFu, 0xFFu};

		/* Wait 50ms before service call */
		if (self->_timer_ms < 50u) {
			break;
		}

		_lcf_sr_push_tx(self, data, 8u);

		self->_timeout_ms = 0u;
		self->_state = LCF_SR_STATE_UDS_CALL_SERVICE1_RESPONSE;
		break;
	}

	case LCF_SR_STATE_UDS_CALL_SERVICE1_RESPONSE: {
		uint8_t expec[8] = {0x03u, 0x71u, 0x03u, 0x02u,
				    0xFFu, 0xFFu, 0xFFu, 0xFFu};

		self->_timeout_ms += delta_time_ms;

		if (!_lcf_sr_validate_response(self, expec, sizeof(expec))) {
			break;
		}

		self->_timer_ms = 0u;
		self->_state = LCF_SR_STATE_UDS_SESSION_DEFAULT;

		break;
	}

	case LCF_SR_STATE_UDS_SESSION_DEFAULT: {
		uint8_t data[8] = {0x02u, 0x10u, 0x81u, 0xFFu,
				   0xFFu, 0xFFu, 0xFFu, 0xFFu};

		/* Wait 50ms before service call */
		if (self->_timer_ms < 50u) {
			break;
		}

		_lcf_sr_push_tx(self, data, 8u);

		self->_timeout_ms = 0u;
		self->_state = LCF_SR_STATE_UDS_SESSION_DEFAULT_RESPONSE;
		break;
	}

	case LCF_SR_STATE_UDS_SESSION_DEFAULT_RESPONSE: {
		uint8_t expec[8] = {0x02u, 0x50u, 0x81u, 0xFFu,
				    0xFFu, 0xFFu, 0xFFu, 0xFFu};

		self->_timeout_ms += delta_time_ms;

		if (!_lcf_sr_validate_response(self, expec, sizeof(expec))) {
			break;
		}

		self->_timer_ms = 0u;
		lcf_sr_stop(self); /* Final step */
		self->_status = LCF_SR_STATUS_SUCCEED;

		break;
	}

	default:
		break;
	}
}

void lcf_sr_step(struct lcf_sr *self, uint32_t delta_time_ms)
{
	if (self->_state != (uint8_t)LCF_SR_STATE_STOPPED) {
		_lcf_sr_step(self, delta_time_ms);
	}
}
