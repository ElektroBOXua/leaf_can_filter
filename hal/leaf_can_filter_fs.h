/* WARNING: NON-MISRA COMPLIANT (TODO) */
#pragma once

#include "LittleFS.h"
#include "leaf_can_filter.h"
#include "target.gen.h"

#define LEAF_CAN_FILTER_FS_THROTTLE_TIME_MS 60000

/******************************************************************************
 * GLOBALS
 *****************************************************************************/
/* Set global variable that indicates filesystem is corrupted. */
bool leaf_can_filter_fs_is_corrupted = false;
bool leaf_can_filter_fs_is_commited  = false; /* Settings changed */

/******************************************************************************
 * PRIVATE
 *****************************************************************************/
void _leaf_can_filter_fs_save(struct leaf_can_filter *self)
{
	File f = LittleFS.open("/settings.bin", "w");

	printf("[FILESYSTEM] writing settings...\n");

	if (!f) {
		printf("[FILESYSTEM] failed opening settings for write\n");	
	} else {
		f.write((uint8_t*)&self->settings,
			sizeof(struct leaf_can_filter_settings));

		f.close();
	}

	leaf_can_filter_fs_is_commited = false;
}

void save_version_on_first_start() 
{
	File f;

	f = LittleFS.open("/version.txt", "r");
	if (f) {
		String current_ver = f.readString();
		f.close();

		printf("System already initialized.\n");

		printf("Stored version: %s\n", current_ver.c_str());
		printf("Loaded version: %s\n", __CAN_FILTER_VERSION__);

		if (!strstr(current_ver.c_str(), __CAN_FILTER_VERSION__))
		{
			printf("Version difference spotted!\n");
		} else {
			printf("Version didn't change since last boot.\n");
			return;
		}
	}

	printf("Writing version: %s\n", __CAN_FILTER_VERSION__);
    
	f = LittleFS.open("/version.txt", "w");
	if (!f) {
		printf("Failed to create version file\n");
		return;
	}
    
	f.print(__CAN_FILTER_VERSION__); 
	f.close();
}

/******************************************************************************
 * PUBLIC
 *****************************************************************************/
void leaf_can_filter_fs_save(struct leaf_can_filter *self)
{
	leaf_can_filter_fs_is_commited = true;
}

void leaf_can_filter_fs_load(struct leaf_can_filter *self)
{
	File f;

	printf("[FILESYSTEM] reading settings...\n");
	
	if (!LittleFS.exists("/settings.bin")) {
		printf("[FILESYSTEM] missing settings (probably first run)");

		_leaf_can_filter_fs_save(self);
	}

	f = LittleFS.open("/settings.bin", "r");
	
	if (!f) {
		printf("[FILESYSTEM] failed opening settings for read\n");
		leaf_can_filter_fs_is_corrupted = true;
	} else {
		f.read((uint8_t*)&self->settings,
			sizeof(struct leaf_can_filter_settings));

		f.close();
	}

	save_version_on_first_start();

	/* Set capacity */
	chgc_set_full_cap_kwh(&self->_chgc,
			     self->settings.capacity_override_kwh);
	chgc_set_full_cap_voltage_V(&self->_chgc,
				    self->settings.capacity_full_voltage_V);

	chgc_set_initial_cap_kwh(&self->_chgc,
			     self->settings.capacity_remaining_kwh);
}

void leaf_can_filter_fs_init(struct leaf_can_filter *self)
{
	if(!LittleFS.begin(true)) {
		printf("[FILESYSTEM] failed to mount (corrupted)\n");
		leaf_can_filter_fs_is_corrupted = true;
	}
	
	leaf_can_filter_fs_load(self);
}

void leaf_can_filter_fs_update(struct leaf_can_filter *self,
			       uint32_t delta_time_ms)
{
	/* Throttle frequent updates to prevent early flash wearing */
	static int32_t update_throttle_ms = 0;

	if (update_throttle_ms > 0) {
		update_throttle_ms -= delta_time_ms;
	}

	if (leaf_can_filter_fs_is_commited && update_throttle_ms <= 0) {
		_leaf_can_filter_fs_save(self);
		
		update_throttle_ms = LEAF_CAN_FILTER_FS_THROTTLE_TIME_MS;
	}

	/* Save remaining kwh onto flash */
	if (self->settings.capacity_override_enabled) {
		self->settings.capacity_remaining_kwh =
			chgc_get_remain_cap_kwh(&self->_chgc);

		leaf_can_filter_fs_save(self);
	}
}
