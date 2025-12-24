/* ISO-TP leaf can filter SOH reset hardcoded emulation */
#define LCF_SR_HEARDBEAT_RATE_MS 2000
#define LCF_SR_TX_ID 0x79Bu
#define LCF_SR_RX_ID 0x7BBu

enum lcf_sr_state {
	LCF_SR_STATE_STOPPED,
	LCF_SR_STATE_IDLE,

	/*LCF_SR_STATE_UDS_SESSION_START
	LCF_SR_STATE_UDS_SESSION_START_RESPONSE*/

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
	LCF_SR_STATE_UDS_SESSION_DEFAULT_RESPONSE,

	/* Await transmission */
	LCF_SR_STATE_TX_AWAIT
};

struct lcf_sr {
	uint8_t _state;

	uint32_t _heartbeat_clock_ms;

	struct leaf_can_filter_frame _tx;
	bool _has_tx;
	struct leaf_can_filter_frame _rx;
	bool _has_rx;
};

void lcf_sr_init(struct lcf_sr *self)
{
	self->state = 0u;

	self->_heartbeat_clock_ms = LCF_SR_HEARDBEAT_RATE_MS;

	self->_has_tx = false;
	self->_has_rx = false;
}

/** Push RX CAN frame for processing, returns false if busy. */
bool lcf_sr_push_frame(struct lcf_sr *self, struct leaf_can_filter_frame *f)
{
	bool result = false;

	if (!self->_has_rx) {
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

void _lcf_sr_step(struct lcf_sr *self, uint32_t delta_time_ms)
{
	/* Switch to heartbeat if timeout and not doing anything */
	if (self->_heartbeat_clock_ms >= LCF_SR_HEARDBEAT_RATE_MS) {
		self->_heartbeat_clock_ms = 0u;

		if (self->_state == LCF_SR_STATE_IDLE) {
			self->state = LCF_SR_STATE_UDS_TESTER_PRESENT;
		}
	}


	switch (self->_state) {
	case LCF_SR_STATE_UDS_TESTER_PRESENT: {
		uint8_t data[8] = {0x02u, 0x3Eu, 0x01u, 0xFFu
				   0xFFu, 0xFFu, 0xFFu, 0xFFu};
		
		self->_tx.id   = LCF_SR_TX_ID;
		self->_tx.len  = 8u;
		memcpy(self->_tx.data, data, 8u);

		self->state = LCF_SR_STATE_TX_AWAIT;
		self->_has_tx = true;
	}
}

void lcf_sr_step(struct lcf_sr *self, uint32_t delta_time_ms)
{
	if (self->_state != LCF_SR_STATE_STOPPED) {
		_lcf_sr_step(self, delta_time_ms);
	}
}
