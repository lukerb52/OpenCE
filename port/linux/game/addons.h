/*
ADDONS.H

Add-ons (docs/addons.md): the tags of the .cevehicle files of the mutators
that are on, added to a map's as it loads (addons.c).
*/

#ifndef __ADDONS_H
#define __ADDONS_H
#pragma once

/* ---------- prototypes/ADDONS.C */

/* after the map named map_name's tags loaded and were checked, before the
menus' tags are added (cache_files.c): the add-ons' tags added to them, the
seats their riders need added to the map's animation graphs, and their
vehicles placed in the scenario (scenario_index) */
void addons_tags_loaded(
	char const *map_name,
	long scenario_index);
/* before the map's tags go (scenario_tags_unload), after the menus' tags: all
of it taken back */
void addons_tags_unloaded(
	void);

/* whether the vehicle definition is an add-on's (game_engine_remap_vehicle
keeps it) */
boolean addons_vehicle(
	long definition_index);
/* whether the bitmap is an add-on's, drawn as a Custom Edition map's are
(xbox_texture_cache.c) */
struct bitmap_data;
boolean addons_bitmap_contains(
	struct bitmap_data const *bitmap);
/* a read of an add-on's tag's pixels or samples (cache_file_read), made at
once from its file: FALSE if the tag is not an add-on's */
boolean addons_read(
	long tag_index,
	long offset,
	long size,
	void *buffer);

#endif // __ADDONS_H
