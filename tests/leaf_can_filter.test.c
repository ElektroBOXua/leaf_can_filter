#include <stdio.h>
#include "leaf_can_filter.h"

void leaf_can_filter_test()
{
	int i;

	struct leaf_can_filter fi;
	struct leaf_can_filter_frame frame;
	
	frame.id = 1468U;
	frame.len = 8U;
	frame.data[0] = 0;
	frame.data[1] = 0;
	frame.data[2] = 0;
	frame.data[3] = 0;
	frame.data[4] = 0;
	frame.data[5] = 0;
	frame.data[6] = 0;
	frame.data[7] = 0;
	
	leaf_can_filter_init(&fi);
	leaf_can_filter_process_frame(&fi, &frame);
	leaf_can_filter_update(&fi);

	for (i = 0; i < 8; i++) {
		printf("0x%02X ", frame.data[i]);
	}

	printf("\n");
}

int main()
{
	leaf_can_filter_test();
	return 0;
}
