#include "../../leaf_can_filter.h"
#include "simple_log_reader.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>

#define SYS_TIMESTAMP_US (clock() / (CLOCKS_PER_SEC / 1000000.0f))
#define SYS_TIMESTAMP_MS (clock() / (CLOCKS_PER_SEC / 1000.0f))

void simple_print_frame(struct simple_log_reader *self)
{
	int i;
			
	printf("%011u %08X %02X %i",
	       self->_frame.timestamp_us, self->_frame.id,
	       self->_frame.flags, self->_frame.len);
	       
	for (i = 0; i < self->_frame.len; i++)
		printf(" %02X", self->_frame.data[i]);
	printf("\n");
}

struct leaf_can_filter fi;

void log_player_leaf_can_filter_init()
{
	leaf_can_filter_init(&fi);
	fi.settings.bypass = false; /* disable bypass */
	fi.settings.capacity_override_enabled = false;
	
	chgc_set_full_cap_kwh(&fi._chgc, 24.0f);
	chgc_set_full_cap_voltage_V(&fi._chgc, 500.0f);
	chgc_set_initial_cap_kwh(&fi._chgc, 10.0f);

	printf("\033[3J\033[H\033[2J");
}

void leaf_can_filter_print_variables()
{
	char buf[1024];
	buf[0] = '\0';

	sprintf(buf, "\033[H");
	sprintf(buf + strlen(buf), "_version:           %u\n",
		fi._version);

	sprintf(buf + strlen(buf), "voltage_V:          %f\n",
		fi._bms_vars.voltage_V);

	sprintf(buf + strlen(buf), "current_A:          %f\n",
		fi._bms_vars.current_A);

	sprintf(buf + strlen(buf), "\033[K\n");

	sprintf(buf + strlen(buf), "full_capacity_gd:   %u        \n",
		(fi._bms_vars.full_capacity_wh - 250) / 80);
	sprintf(buf + strlen(buf), "full_capacity_wh:   %u\n",
		fi._bms_vars.full_capacity_wh);

	sprintf(buf + strlen(buf), "remain_capacity_gd: %u\n",
		fi._bms_vars.remain_capacity_wh / 80);
	sprintf(buf + strlen(buf), "remain_capacity_wh: %u\n",
		fi._bms_vars.remain_capacity_wh);

	sprintf(buf + strlen(buf), "\n");
	sprintf(buf + strlen(buf), "\033[K\n");

	printf("%s", buf);
}

int main()
{
	int c;
	struct simple_log_reader c_inst;
	uint32_t prev_time_us = 0;

	char *files[] = {
		"../../logs/aze0/ch1_20250804_161600_bypass.csv",
		"../../logs/24_11_2025_ze0/leaf_2012_orig_start_stop.csv",
		"../../logs/24_11_2025_ze0/leaf_2012_charge.csv"
	};

	FILE *file = fopen(files[1], "r");
	
	assert(file);
	
	simple_log_reader_init(&c_inst);

	log_player_leaf_can_filter_init();

	c = getc(file);
	while (!feof(file)) {
		enum simple_log_reader_event ev;

		if (SYS_TIMESTAMP_US <= prev_time_us) {
			continue;
		}

		ev = simple_log_reader_putc(&c_inst, c);

		if (ev == SIMPLE_LOG_READER_EVENT_FRAME_READY) {
			struct leaf_can_filter_frame f;

			f.id = c_inst._frame.id;
			f.len = c_inst._frame.len;
			memcpy(f.data, c_inst._frame.data, f.len);

			/* simple_print_frame(&c_inst); */

			leaf_can_filter_process_frame(&fi, &f);
			leaf_can_filter_update(&fi, (c_inst._frame.timestamp_us - prev_time_us) / 1000);

			leaf_can_filter_print_variables();

			prev_time_us = c_inst._frame.timestamp_us;
		}

		if (ev == SIMPLE_LOG_READER_EVENT_ERROR) {
			printf("err, state: %i, flags: %i\n",
				c_inst._estate, c_inst._eflags); fflush(0);
		}

		c = getc(file);
	}

	printf("FINISHED, TOTAL_FRAMES: %llu\n", c_inst._total_frames);

	return 0;
}
