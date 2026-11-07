/*
ADDONS.H

Add-ons (docs/addons.md): the tags of the .cevehicle and .ceweapon files of
the mutators that are on, added to a map's as it loads (addons.c).
*/

#ifndef __ADDONS_H
#define __ADDONS_H
#pragma once

/* ---------- prototypes/ADDONS.C */

/* after the map named map_name's tags loaded and were checked, before the
menus' tags are added (cache_files.c): the add-ons' tags added to them, the
seats their riders need added to the map's animation graphs, their vehicles
placed in the scenario (scenario_index) and their weapons spawned there */
void addons_tags_loaded(
	char const *map_name,
	long scenario_index);
/* before the map's tags go (scenario_tags_unload), after the menus' tags: all
of it taken back */
void addons_tags_unloaded(
	void);

/* whether the vehicle definition is an add-on's (game_engine_remap_vehicle
keeps it), and whether the weapon definition is (game_engine_remap_weapon
keeps it in the weapon sets of every kind) */
boolean addons_vehicle(
	long definition_index);
boolean addons_weapon(
	long definition_index);
/* an add-on vehicle's place among the vehicle add-ons of the mutators on
(the gametypes' custom vehicles' rows), or NONE for another vehicle */
short addons_vehicle_slot(
	long definition_index);
/* the weapon of the weapon add-on at the place among those of the mutators
on (a custom loadout's), or NONE if this map has it not */
long addons_weapon_definition(
	short slot);
/* the messaging icons (a bitmap) of the add-on whose tags hold the address
(a weapon's HUD interface's messaging icon: its pickup's picture, a sprite
of the map's it was made from), or NONE for the map's own (hud_messaging.c) */
long addons_messaging_icons(
	void const *address);
/* whether the bitmap is an add-on's, drawn as a Custom Edition map's are
(xbox_texture_cache.c) */
struct bitmap_data;
boolean addons_bitmap_contains(
	struct bitmap_data const *bitmap);
/* a bitmap's pixels read into buffer at once from its add-on's file, and
made what this build draws, as a Custom Edition map's are (the texture
cache): FALSE if it is not an add-on's */
boolean addons_bitmaps_read(
	struct bitmap_data *bitmap,
	void *buffer);
/* a read of an add-on's tag's pixels or samples (cache_file_read), made at
once from its file: FALSE if the tag is not an add-on's */
boolean addons_read(
	long tag_index,
	long offset,
	long size,
	void *buffer);

#endif // __ADDONS_H
