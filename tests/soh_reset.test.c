#include "leaf_can_filter.h"
#include <stdio.h>

struct lcf_sr sr;
struct lcf_sr sr_timeout_sav;

struct example_frame {
	double timestamp_s;
	uint32_t id;
	uint8_t  dlc;
	uint8_t  data[8u];
};

struct example_frame example_frames[14] = {
	{ 0001.205795, 0x0000079Bu, 8u, { 0x02u, 0x3Eu, 0x01u, 0xFFu,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	{ 0001.212579, 0x000007BBu, 8u, { 0x01u, 0x7Eu, 0xFFu, 0xFFu,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	{ 0003.356426, 0x0000079Bu, 8u, { 0x02u, 0x3Eu, 0x01u, 0xFFu,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	{ 0003.363399, 0x000007BBu, 8u, { 0x01u, 0x7Eu, 0xFFu, 0xFFu,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	/* BEGIN Enter privileged mode (manually inserted) */
	{ 0004.008432, 0x0000079Bu, 8u, { 0x02u, 0x10u, 0xC0u, 0xFFu,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	{ 0004.012318, 0x000007BBu, 8u, { 0x02u, 0x50u, 0xC0u, 0xFFu,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },
	/* END   Enter privileged mode (manually inserted) */

	{ 0004.088432, 0x0000079Bu, 8u, { 0x03u, 0x31u, 0x03u, 0x00u,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	{ 0004.092318, 0x000007BBu, 8u, { 0x03u, 0x71u, 0x03u, 0x01u,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	{ 0004.623995, 0x0000079Bu, 8u, { 0x03u, 0x31u, 0x03u, 0x01u,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	{ 0004.635545, 0x000007BBu, 8u, { 0x03u, 0x71u, 0x03u, 0x02u,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	{ 0004.850283, 0x0000079Bu, 8u, { 0x02u, 0x10u, 0x81u, 0xFFu,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	{ 0004.852249, 0x000007BBu, 8u, { 0x02u, 0x50u, 0x81u, 0xFFu,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	{ 0007.001728, 0x0000079Bu, 8u, { 0x02u, 0x3Eu, 0x01u, 0xFFu,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } },

	{ 0007.003310, 0x000007BBu, 8u, { 0x01u, 0x7Eu, 0xFFu, 0xFFu,
					  0xFFu, 0xFFu, 0xFFu, 0xFFu } }
};

void lcf_sr_test_print_can_frame(struct leaf_can_filter_frame *f)
{
	uint8_t i;
			
	printf("%08X %u: ", f->id, f->len);
	       
	for (i = 0; i < f->len; i++) {
		printf("%02X ", f->data[i]);
	}

	printf("\n");
}

void lcf_sr_test_heartbeat(struct lcf_sr *self)
{
	uint8_t data[8] = {0x02u, 0x3Eu, 0x01u, 0xFFu,
			   0xFFu, 0xFFu, 0xFFu, 0xFFu};

	struct leaf_can_filter_frame f;
	(void)memset(&f, 0u, sizeof(struct leaf_can_filter_frame));
	assert(memcmp(f.data, data, 8u));

	assert(lcf_sr_pop_frame(self, &f) == false);

	lcf_sr_step(self, 0u);

	assert(lcf_sr_pop_frame(self, &f) == false);

	lcf_sr_start(self);

	assert(lcf_sr_pop_frame(self, &f) == false);

	lcf_sr_step(self, 0u);

	assert(lcf_sr_pop_frame(self, &f) == true);
	assert(!memcmp(f.data, data, 8u));
	assert(lcf_sr_pop_frame(self, &f) == false);

	/* Put heartbeat positive response */
	(void)memset(&f, 0xFFu, sizeof(struct leaf_can_filter_frame));
	f.id  = LCF_SR_RX_ID;
	f.len = 8u;
	f.data[0] = 0x01;
	f.data[1] = 0x7E;
	assert(lcf_sr_push_frame(self, &f) == true);
	lcf_sr_step(self, 0u);

	/* Repeat heartbeat after rate again */
	lcf_sr_step(self, LCF_SR_HEARTBEAT_RATE_MS - 1u);
	assert(lcf_sr_pop_frame(self, &f) == false);

	lcf_sr_step(self, 1u);
	assert(lcf_sr_pop_frame(self, NULL) == true);
	assert(lcf_sr_pop_frame(self, NULL) == false);

	/* Put positive response again */
	assert(lcf_sr_push_frame(self, &f) == true);
	lcf_sr_step(self, 0u);

	/* Wait, so transition to LCF_SR_STATE_UDS_CALL_SERVICE0 occur */
	lcf_sr_step(self, 500u);
}

void lcf_sr_test_privil_enter(struct lcf_sr *self)
{
	uint8_t tx[8] = {0x02u, 0x10u, 0xC0u, 0xFFu,
			 0xFFu, 0xFFu, 0xFFu, 0xFFu};

	uint8_t rx[8] = {0x02u, 0x50u, 0xC0u, 0xFFu,
			 0xFFu, 0xFFu, 0xFFu, 0xFFu};

	struct leaf_can_filter_frame f;
	(void)memset(&f, 0u, sizeof(struct leaf_can_filter_frame));

	assert(lcf_sr_pop_frame(self, &f) == false);

	/* eval LCF_SR_STATE_UDS_CALL_*** */
	lcf_sr_step(self, 0u);
	assert(lcf_sr_pop_frame(self, &f) == true);
	assert(!memcmp(f.data, tx, 8u));
	assert(lcf_sr_pop_frame(self, &f) == false);

	/* Put positive response */
	f.id  = LCF_SR_RX_ID;
	f.len = 8u;
	memcpy(f.data, rx, 8u);
	assert(lcf_sr_push_frame(self, &f) == true);
	/* eval LCF_SR_STATE_UDS_CALL_***_RESPONSE */
	lcf_sr_step(self, 0u);
}

void lcf_sr_test_service0(struct lcf_sr *self)
{
	uint8_t tx[8] = {0x03u, 0x31u, 0x03u, 0x00u,
			 0xFFu, 0xFFu, 0xFFu, 0xFFu};

	uint8_t rx[8] = {0x03u, 0x71u, 0x03u, 0x01u,
			 0xFFu, 0xFFu, 0xFFu, 0xFFu};

	struct leaf_can_filter_frame f;
	(void)memset(&f, 0u, sizeof(struct leaf_can_filter_frame));

	assert(lcf_sr_pop_frame(self, &f) == false);

	/* eval LCF_SR_STATE_UDS_CALL_*** */
	lcf_sr_step(self, 0u);
	assert(lcf_sr_pop_frame(self, &f) == true);
	assert(!memcmp(f.data, tx, 8u));
	assert(lcf_sr_pop_frame(self, &f) == false);

	/* Put positive response */
	f.id  = LCF_SR_RX_ID;
	f.len = 8u;
	memcpy(f.data, rx, 8u);
	assert(lcf_sr_push_frame(self, &f) == true);
	/* eval LCF_SR_STATE_UDS_CALL_***_RESPONSE */
	lcf_sr_step(self, 0u);
}

void lcf_sr_test_service1(struct lcf_sr *self)
{
	uint8_t tx[8] = {0x03u, 0x31u, 0x03u, 0x01u,
			 0xFFu, 0xFFu, 0xFFu, 0xFFu};

	uint8_t rx[8] = {0x03u, 0x71u, 0x03u, 0x02u,
			 0xFFu, 0xFFu, 0xFFu, 0xFFu};

	struct leaf_can_filter_frame f;
	(void)memset(&f, 0u, sizeof(struct leaf_can_filter_frame));

	assert(lcf_sr_pop_frame(self, &f) == false);

	/* eval LCF_SR_STATE_UDS_CALL_*** */
	lcf_sr_step(self, LCF_SR_RX_TIMEOUT_MS);
	assert(lcf_sr_pop_frame(self, &f) == true);
	assert(!memcmp(f.data, tx, 8u));
	assert(lcf_sr_pop_frame(self, &f) == false);

	/* Set savepoint so test timeout later */
	sr_timeout_sav = *self;

	/* Put positive response */
	f.id  = LCF_SR_RX_ID;
	f.len = 8u;
	memcpy(f.data, rx, 8u);
	assert(lcf_sr_push_frame(self, &f) == true);
	/* eval LCF_SR_STATE_UDS_CALL_***_RESPONSE */
	lcf_sr_step(self, 0u);
}

void lcf_sr_test_uds_session_def(struct lcf_sr *self)
{
	uint8_t tx[8] = {0x02u, 0x10u, 0x81u, 0xFFu,
			 0xFFu, 0xFFu, 0xFFu, 0xFFu};

	uint8_t rx[8] = {0x02u, 0x50u, 0x81u, 0xFFu,
			 0xFFu, 0xFFu, 0xFFu, 0xFFu};

	struct leaf_can_filter_frame f;
	(void)memset(&f, 0u, sizeof(struct leaf_can_filter_frame));

	/* eval LCF_SR_STATE_UDS_CALL_*** */
	assert(lcf_sr_pop_frame(self, &f) == false);
	lcf_sr_step(self, LCF_SR_RX_TIMEOUT_MS);

	assert(lcf_sr_pop_frame(self, &f) == true);
	assert(!memcmp(f.data, tx, 8u));
	assert(lcf_sr_pop_frame(self, &f) == false);

	/* Put positive response */
	f.id  = LCF_SR_RX_ID;
	f.len = 8u;
	memcpy(f.data, rx, 8u);
	assert(lcf_sr_push_frame(self, &f) == true);
	/* eval LCF_SR_STATE_UDS_CALL_***_RESPONSE */
	lcf_sr_step(self, 0u);
}

void lcf_sr_test_timeout(struct lcf_sr *self)
{
	lcf_sr_step(self, LCF_SR_RX_TIMEOUT_MS - 1u);
	assert(self->_state != LCF_SR_STATE_STOPPED);

	lcf_sr_step(self, 1);
	assert(self->_state == LCF_SR_STATE_STOPPED);

	assert(lcf_sr_get_status(self) == LCF_SR_STATUS_TIMEOUT);
}

void lcf_sr_test_example_log(struct lcf_sr *self)
{
	struct leaf_can_filter_frame f;

	uint32_t i = 0u;
	uint32_t timer_ms = 0u;

	lcf_sr_init(self);
	lcf_sr_start(self);

	for (i = 0u; i < 14; i++) {
		uint32_t timestamp_s = (example_frames[i].timestamp_s * 1000);

		while (timer_ms <= timestamp_s) {
			lcf_sr_step(self, 1);

			if (lcf_sr_pop_frame(self, &f)) {
				lcf_sr_test_print_can_frame(&f);
			}

			timer_ms++;
		}

		if (example_frames[i].id == LCF_SR_TX_ID) {
			continue; /* Skip TX frames */
		}

		f.id  = example_frames[i].id;
		f.len = example_frames[i].dlc;
		memcpy(f.data, example_frames[i].data, 8u);
		assert(lcf_sr_push_frame(self, &f) == true);
		lcf_sr_test_print_can_frame(&f);

		/* printf("timer: %u\n", timer_ms); */
	}

	assert(lcf_sr_get_status(self) == LCF_SR_STATUS_SUCCEED);
}

int main()
{
	lcf_sr_init(&sr);
	lcf_sr_test_heartbeat(&sr);
	lcf_sr_test_privil_enter(&sr);
	lcf_sr_test_service0(&sr);
	lcf_sr_test_service1(&sr);
	lcf_sr_test_uds_session_def(&sr);
	lcf_sr_test_timeout(&sr_timeout_sav);
	lcf_sr_test_example_log(&sr);

	printf("\x1B[32mTest succeed\x1B[0m\n");
	return 0;
}
