/*
The mutators' tag edits (port/linux/game/mutator_tags.c) in a fake map's tags,
made from mutator.toml by port/linux/src/port_mutators.c: the built-in
accurized MA5B's every value, undone; tags, elements and tags named that a
map lacks passed over; edits refused for a field, value or path this build
does not take; and edits' bytes that are not edits' refused whole.
*/

#include "harness.h"
#include <math.h>
#include <stddef.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
/* (the library is C11: the harness builds C99) */
#define static_assert _Static_assert
#include "tomlc17.inc"
#pragma clang diagnostic pop

typedef unsigned short word;
#define csmemcpy memcpy
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define NUMBEROF(array) (sizeof(array) / sizeof(array[0]))
#define _pi ((real)3.14159265358979323846f)
enum { _error_log };
static int logged_errors;
#define error(kind, ...) ((void)(kind), logged_errors++)
#define console_printf(clear, ...) ((void)(clear))
#include "enums.inc"
enum
{
	WEAPON_DEFINITION_TAG = 'weap',
	PROJECTILE_DEFINITION_TAG = 'proj',
	SOUND_DEFINITION_TAG = 'snd!',
	EFFECT_DEFINITION_TAG = 'effe',
	HUD_GROUP_TAG = 'wphi',
};
struct real_orientation;

/* the tags' structures: the fields mutator_tags.c sets, by the game's names */
struct tag_reference { unsigned long group_tag; char *name; long name_length; long index; };
struct tag_block { long count; void *address; void *definition; };
struct weapon_trigger_definition
{
	unsigned long flags;
	real projectile_error_angle_lower_bound;
	real projectile_error_angle_upper_bound;
	short rounds_between_tracers;
	short pad;
};
struct weapon_definition
{
	struct
	{
		long flags;
		short pad;
		short zoom_level_count;
		real zoom_magnification_minimum;
		real zoom_magnification_maximum;
		struct tag_reference zoom_in_sound;
		struct tag_reference zoom_out_sound;
		struct tag_block triggers;
	} weapon;
};
struct projectile_material_response_definition
{
	word flags;
	short default_response;
	struct tag_reference default_effect;
	short potential_response;
	word potential_flags;
	real potential_skip_fraction;
	real potential_angle_lower_bound;
	real potential_angle_upper_bound;
	struct tag_reference potential_effect;
};
struct projectile_definition
{
	struct
	{
		real maximum_range;
		struct tag_block material_responses;
	} projectile;
};
struct weapon_hud_interface_definition
{
	struct tag_reference parent_hud;
	struct tag_block crosshairs;
	unsigned long valid_crosshair_types_flags;
	struct tag_block screen_effects;
};
struct hud_screen_effect_definition { word mask_flags; word pad; struct tag_reference mask_fullscreen; };

/* the map: its tags, by index (with a salt, as the game's) */
#define CROSSHAIR_BYTES 0x68
enum
{
	_tag_rifle, _tag_bullet, _tag_rifle_hud, _tag_pistol_hud, _tag_zoom_in, _tag_zoom_out, _tag_stone, _tag_hollow,
	_tag_thin, _tag_thick, _tag_ice, NUMBER_OF_TAGS
};
#define TAG_INDEX(index) ((long)(0xE1740000UL | (unsigned long)(index)))
static struct { unsigned long group_tag; char name[80]; void *data; int present; } tags[NUMBER_OF_TAGS];
static struct weapon_definition rifle;
static struct weapon_trigger_definition triggers[2];
static struct projectile_definition bullet;
static struct projectile_material_response_definition responses[NUMBER_OF_MATERIAL_TYPES + 1];
static struct weapon_hud_interface_definition rifle_hud, pistol_hud;
static byte rifle_crosshairs[1][CROSSHAIR_BYTES], pistol_crosshairs[3][CROSSHAIR_BYTES];
static struct hud_screen_effect_definition pistol_effects[1];
static int other_tag_data;

static long tag_loaded(long group_tag, char const *name)
{
	int index;

	for (index = 0; index < NUMBER_OF_TAGS; index++)
	{
		if (tags[index].present && tags[index].group_tag == (unsigned long)group_tag && !strcasecmp(tags[index].name, name))
			return TAG_INDEX(index);
	}
	return NONE;
}
static void *tag_get(long group_tag, long tag_index)
{
	long index = DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index);

	CHECK(index < NUMBER_OF_TAGS && tags[index].present && tags[index].group_tag == (unsigned long)group_tag &&
		(unsigned long)tag_index == (unsigned long)TAG_INDEX(index), "tag_get of a tag the map has not");
	return tags[index].data;
}
static char *tag_get_name(long tag_index)
{
	return tags[DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index)].name;
}

/* the platform layer's, as test_port_mutators.py fakes it (no mutators folder) */
struct fake_folder { char const *name; char const *text; unsigned long size; };
static struct fake_folder folders[32];
static int folder_count, listed, reads, logged;
static char setting[300], written[300];

#include "mutators_header.inc"
#include "mutator_tags.inc"
#include "port_mutators_source.inc"

static void tag(int index, unsigned long group_tag, char const *name, void *data)
{
	tags[index].group_tag = group_tag;
	snprintf(tags[index].name, sizeof(tags[index].name), "%s", name);
	tags[index].data = data;
	tags[index].present = 1;
}

static void folder(char const *name, char const *text)
{
	folders[folder_count].name = name;
	folders[folder_count].text = text;
	folders[folder_count++].size = text ? (unsigned long)strlen(text) : 0;
}

/* the stock map's tags, as the edits find them */
static void world(void)
{
	int index;

	memset(tags, 0, sizeof(tags));
	memset(&rifle, 0, sizeof(rifle));
	memset(triggers, 0x5A, sizeof(triggers));
	triggers[0].projectile_error_angle_lower_bound = 0.0349066f;
	triggers[0].projectile_error_angle_upper_bound = 0.113446f;
	triggers[0].rounds_between_tracers = 3;
	rifle.weapon.zoom_magnification_minimum = 1.f;
	rifle.weapon.zoom_in_sound.index = NONE;
	rifle.weapon.zoom_out_sound.index = NONE;
	rifle.weapon.triggers.count = 1;
	rifle.weapon.triggers.address = triggers;
	memset(&bullet, 0, sizeof(bullet));
	memset(responses, 0, sizeof(responses));
	for (index = 0; index < NUMBER_OF_MATERIAL_TYPES + 1; index++)
	{
		responses[index].default_response = _projectile_material_response_detonate;
		responses[index].default_effect.index = NONE;
		responses[index].potential_effect.index = NONE;
	}
	responses[_material_grunt].default_response = _projectile_material_response_overpenetrate;
	bullet.projectile.maximum_range = 40.f;
	bullet.projectile.material_responses.count = NUMBER_OF_MATERIAL_TYPES;
	bullet.projectile.material_responses.address = responses;
	memset(&rifle_hud, 0, sizeof(rifle_hud));
	memset(&pistol_hud, 0, sizeof(pistol_hud));
	memset(rifle_crosshairs, 'r', sizeof(rifle_crosshairs));
	for (index = 0; index < 3; index++)
		memset(pistol_crosshairs[index], '0' + index, CROSSHAIR_BYTES);
	memset(pistol_effects, 0, sizeof(pistol_effects));
	pistol_effects[0].mask_flags = 1;
	rifle_hud.valid_crosshair_types_flags = 1;
	rifle_hud.crosshairs.count = 1;
	rifle_hud.crosshairs.address = rifle_crosshairs;
	pistol_hud.valid_crosshair_types_flags = 3;
	pistol_hud.crosshairs.count = 3;
	pistol_hud.crosshairs.address = pistol_crosshairs;
	pistol_hud.screen_effects.count = 1;
	pistol_hud.screen_effects.address = pistol_effects;
	tag(_tag_rifle, WEAPON_DEFINITION_TAG, "weapons\\assault rifle\\assault rifle", &rifle);
	tag(_tag_bullet, PROJECTILE_DEFINITION_TAG, "weapons\\assault rifle\\bullet", &bullet);
	tag(_tag_rifle_hud, HUD_GROUP_TAG, "weapons\\assault rifle\\assault rifle", &rifle_hud);
	tag(_tag_pistol_hud, HUD_GROUP_TAG, "weapons\\pistol\\pistol", &pistol_hud);
	tag(_tag_zoom_in, SOUND_DEFINITION_TAG, "sound\\sfx\\weapons\\sniper rifle\\sniper_2x_zoom", &other_tag_data);
	tag(_tag_zoom_out, SOUND_DEFINITION_TAG, "sound\\sfx\\weapons\\sniper rifle\\sniper_10x_zoom", &other_tag_data);
	tag(_tag_stone, EFFECT_DEFINITION_TAG, "weapons\\assault rifle\\effects\\impact stone", &other_tag_data);
	tag(_tag_hollow, EFFECT_DEFINITION_TAG, "weapons\\assault rifle\\effects\\impact metal hollow reflect",
		&other_tag_data);
	tag(_tag_thin, EFFECT_DEFINITION_TAG, "weapons\\assault rifle\\effects\\impact metal thin reflect", &other_tag_data);
	tag(_tag_thick, EFFECT_DEFINITION_TAG, "weapons\\assault rifle\\effects\\impact metal thick reflect",
		&other_tag_data);
	tag(_tag_ice, EFFECT_DEFINITION_TAG, "weapons\\assault rifle\\effects\\impact ice", &other_tag_data);
	mutator_tags_loaded();
}

/* every tag's bytes, to see what an edit changed */
struct snapshot
{
	struct weapon_definition rifle;
	struct weapon_trigger_definition triggers[2];
	struct projectile_definition bullet;
	struct projectile_material_response_definition responses[NUMBER_OF_MATERIAL_TYPES + 1];
	struct weapon_hud_interface_definition rifle_hud, pistol_hud;
};

static void snapshot(struct snapshot *taken)
{
	taken->rifle = rifle;
	memcpy(taken->triggers, triggers, sizeof(triggers));
	taken->bullet = bullet;
	memcpy(taken->responses, responses, sizeof(responses));
	taken->rifle_hud = rifle_hud;
	taken->pistol_hud = pistol_hud;
}

static int unchanged(struct snapshot const *taken)
{
	struct snapshot now;

	snapshot(&now);
	return !memcmp(&now, taken, sizeof(now));
}

/* the tag edits of the mutators game.mutators turns on */
static long edits_of(char const *mutators, unsigned char *edits, long maximum)
{
	snprintf(setting, sizeof(setting), "%s", mutators);
	port_mutators_scan();
	return port_mutators_tag_edits(edits, maximum);
}

static long find(char const *id)
{
	long index;

	for (index = 0; index < port_mutators_count(); index++)
	{
		if (!strcmp(port_mutators.mutators[index].id, id))
			return index;
	}
	return NONE;
}

/* the reason a folder's mutator cannot be played, or "" */
static char const *refusal(char const *id)
{
	static char text[256];

	port_mutators_status(find(id), text, sizeof(text));
	return text;
}

static int degrees(real radians, real expected)
{
	return radians == expected * (_pi / 180.f);
}

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	static unsigned char edits[MAXIMUM_MUTATOR_TAG_EDIT_BYTES];
	struct snapshot stock;
	long size;
	int index;

	CHECK(argc == 2, "case");
	(void)reads;
	(void)logged;
	world();
	snapshot(&stock);
	if (!strcmp(case_name, "ma5b"))
	{
		struct projectile_material_response_definition const *stone = &responses[_material_stone];
		static int const ricochets[] = { _material_stone, _material_hollow_metal, _material_thin_metal,
			_material_thick_metal, _material_ice };
		static int const reflects[] = { _material_force_field, _material_hunter_armor, _material_engineer_force_field,
			_material_cyborg_energy_shield, _material_elite_energy_shield };

		size = edits_of("accurized_ma5b", edits, sizeof(edits));
		CHECK(port_mutators_status(find("accurized_ma5b"), NULL, 0) == _port_mutator_fine, "%s",
			refusal("accurized_ma5b"));
		CHECK(size > 0 && port_mutators.mutators[find("accurized_ma5b")].tag_edit_count == 37, "the edits (%ld)",
			port_mutators.mutators[find("accurized_ma5b")].tag_edit_count);
		CHECK(mutator_tags_play(edits, size), "the built-in mutator's edits refused");
		CHECK(mutator_tags.made == 37 && mutator_tags.passed_over == 0, "made %d, passed over %d", mutator_tags.made,
			mutator_tags.passed_over);
		/* the weapon: the pistol's 2x zoom, the sniper rifle's sounds */
		CHECK(rifle.weapon.zoom_level_count == 1 && rifle.weapon.zoom_magnification_minimum == 2.f &&
			rifle.weapon.zoom_magnification_maximum == 2.f, "the zoom");
		CHECK(rifle.weapon.zoom_in_sound.index == TAG_INDEX(_tag_zoom_in) &&
			rifle.weapon.zoom_in_sound.group_tag == SOUND_DEFINITION_TAG &&
			rifle.weapon.zoom_in_sound.name == tags[_tag_zoom_in].name &&
			rifle.weapon.zoom_in_sound.name_length == (long)strlen(tags[_tag_zoom_in].name) &&
			rifle.weapon.zoom_out_sound.index == TAG_INDEX(_tag_zoom_out), "the zoom's sounds");
		/* the trigger: 0.05 to 3 degrees of error, a tracer each 5 rounds */
		CHECK(degrees(triggers[0].projectile_error_angle_lower_bound, 0.05f) &&
			degrees(triggers[0].projectile_error_angle_upper_bound, 3.f), "the error angles: %g %g",
			triggers[0].projectile_error_angle_lower_bound, triggers[0].projectile_error_angle_upper_bound);
		CHECK(triggers[0].rounds_between_tracers == 5 && triggers[0].flags == stock.triggers[0].flags, "the tracers");
		CHECK(!memcmp(&triggers[1], &stock.triggers[1], sizeof(triggers[1])), "past the block's element");
		/* the bullet: 300 world units, CR's ricochets, the stock's overpenetration */
		CHECK(bullet.projectile.maximum_range == 300.f, "the range");
		for (index = 0; index < (int)NUMBEROF(ricochets); index++)
		{
			struct projectile_material_response_definition const *response = &responses[ricochets[index]];

			CHECK(response->potential_response == _projectile_material_response_reflect &&
				response->potential_skip_fraction == 0.9f && degrees(response->potential_angle_upper_bound, 60.f) &&
				response->potential_angle_lower_bound == 0.f && response->potential_flags == 0 &&
				response->potential_effect.group_tag == EFFECT_DEFINITION_TAG &&
				response->default_response == _projectile_material_response_detonate, "material %d's ricochet",
				ricochets[index]);
		}
		CHECK(stone->potential_effect.index == TAG_INDEX(_tag_stone) &&
			responses[_material_ice].potential_effect.index == TAG_INDEX(_tag_ice) &&
			responses[_material_hollow_metal].potential_effect.index == TAG_INDEX(_tag_hollow), "the ricochets' effects");
		for (index = 0; index < (int)NUMBEROF(reflects); index++)
		{
			CHECK(responses[reflects[index]].default_response == _projectile_material_response_reflect &&
				responses[reflects[index]].potential_response == 0, "material %d's reflection", reflects[index]);
		}
		CHECK(responses[_material_grunt].default_response == _projectile_material_response_overpenetrate &&
			!memcmp(&responses[_material_grunt], &stock.responses[_material_grunt], sizeof(responses[0])) &&
			!memcmp(&responses[NUMBER_OF_MATERIAL_TYPES], &stock.responses[NUMBER_OF_MATERIAL_TYPES],
				sizeof(responses[0])), "a material it does not edit");
		/* the HUD: its own crosshair, then the pistol's scope crosshairs, and its mask */
		CHECK(rifle_hud.valid_crosshair_types_flags == 3, "the crosshair types");
		CHECK(rifle_hud.crosshairs.count == 3 && rifle_hud.crosshairs.address != rifle_crosshairs &&
			!memcmp(rifle_hud.crosshairs.address, rifle_crosshairs[0], CROSSHAIR_BYTES) &&
			!memcmp((byte *)rifle_hud.crosshairs.address + CROSSHAIR_BYTES, pistol_crosshairs[1], CROSSHAIR_BYTES) &&
			!memcmp((byte *)rifle_hud.crosshairs.address + 2 * CROSSHAIR_BYTES, pistol_crosshairs[2], CROSSHAIR_BYTES),
			"the crosshairs");
		CHECK(rifle_hud.screen_effects.count == 1 &&
			!memcmp(rifle_hud.screen_effects.address, pistol_effects, sizeof(pistol_effects)), "the scope's mask");
		CHECK(!memcmp(&pistol_hud, &stock.pistol_hud, sizeof(pistol_hud)) && pistol_crosshairs[1][0] == '1',
			"the pistol's own HUD changed");
		/* played again (the next game on the map): made once, not twice */
		CHECK(mutator_tags_play(edits, size) && rifle_hud.crosshairs.count == 3, "the copy made twice");
		mutator_tags_undo();
		CHECK(unchanged(&stock), "the stock tags not put back");
		CHECK(rifle_hud.crosshairs.address == rifle_crosshairs, "the block's own elements not put back");
		/* (the map's tags loaded again: its own bytes, nothing undone) */
		mutator_tags_play(edits, size);
		world();
		snapshot(&stock);
		mutator_tags_undo();
		CHECK(unchanged(&stock), "edits of a map's tags no longer loaded undone");
	}
	else if (!strcmp(case_name, "missing"))
	{
		/* a map without the pistol's HUD or the sniper rifle's zoom: the rest made */
		tags[_tag_pistol_hud].present = 0;
		tags[_tag_zoom_in].present = 0;
		tags[_tag_ice].present = 0;
		size = edits_of("accurized_ma5b", edits, sizeof(edits));
		CHECK(mutator_tags_play(edits, size), "refused");
		CHECK(mutator_tags.passed_over == 4 && mutator_tags.made == 33, "made %d, passed over %d", mutator_tags.made,
			mutator_tags.passed_over);
		CHECK(rifle.weapon.zoom_in_sound.index == NONE && rifle.weapon.zoom_out_sound.index == TAG_INDEX(_tag_zoom_out),
			"a sound the map has not");
		CHECK(rifle_hud.crosshairs.count == 1 && rifle_hud.screen_effects.count == 0 &&
			rifle_hud.valid_crosshair_types_flags == 3, "a HUD the map has not");
		CHECK(responses[_material_ice].potential_effect.index == NONE &&
			responses[_material_ice].potential_response == _projectile_material_response_reflect, "an effect");
		CHECK(rifle.weapon.zoom_level_count == 1 && bullet.projectile.maximum_range == 300.f, "the others");
		/* a map without the weapon: none of its edits made, nothing touched */
		mutator_tags_undo();
		world();
		tags[_tag_rifle].present = 0;
		mutator_tags_play(edits, size);
		CHECK(rifle.weapon.zoom_level_count == 0 && triggers[0].rounds_between_tracers == 3, "a tag the map has not");
		/* an element past the tag's block */
		mutator_tags_undo();
		world();
		folder("second", "[[tags]]\ngroup = \"weapon\"\ntag = 'weapons\\assault rifle\\assault rifle'\n"
			"[tags.triggers.1]\nrounds_between_tracers = 9\n");
		size = edits_of("second", edits, sizeof(edits));
		CHECK(size > 0 && mutator_tags_play(edits, size) && mutator_tags.passed_over == 1, "refused");
		CHECK(triggers[1].rounds_between_tracers == stock.triggers[1].rounds_between_tracers, "past the block written");
		rifle.weapon.triggers.count = 2;
		CHECK(mutator_tags_play(edits, size) && triggers[1].rounds_between_tracers == 9, "the second trigger");
	}
	else if (!strcmp(case_name, "refused"))
	{
		static struct { char const *id; char const *text; char const *reason; } const cases[] =
		{
			{ "group", "[[tags]]\ngroup = \"biped\"\ntag = 'x'\njump_velocity = 1\n", "group biped" },
			{ "field", "[[tags]]\ngroup = \"weapon\"\ntag = 'x'\nheat_loss_per_second = 1\n", "no field" },
			{ "range", "[[tags]]\ngroup = \"weapon\"\ntag = 'x'\n[tags.triggers.0]\n"
				"projectile_error_angle_upper_bound = 200\n", "from 0 to 180" },
			{ "whole", "[[tags]]\ngroup = \"weapon\"\ntag = 'x'\nzoom_level_count = 1.5\n", "whole number" },
			{ "nan", "[[tags]]\ngroup = \"projectile\"\ntag = 'x'\nmaximum_range = nan\n", "is a number" },
			{ "enum", "[[tags]]\ngroup = \"projectile\"\ntag = 'x'\n[tags.material_responses.stone]\n"
				"default_response = \"explode\"\n", "disappear, detonate" },
			{ "material", "[[tags]]\ngroup = \"projectile\"\ntag = 'x'\n[tags.material_responses.lava]\n"
				"default_response = \"reflect\"\n", "not an element" },
			{ "flags", "[[tags]]\ngroup = \"weapon_hud_interface\"\ntag = 'x'\nvalid_crosshair_types = [\"aim\", "
				"\"laser\"]\n", "aim, zoom" },
			{ "block", "[[tags]]\ngroup = \"weapon\"\ntag = 'x'\ntriggers = 1\n", "is a block" },
			{ "deep", "[[tags]]\ngroup = \"weapon\"\ntag = 'x'\n[tags.zoom_level_count.x]\ny = 1\n",
				"not a field's path" },
			{ "copy", "[[tags]]\ngroup = \"weapon\"\ntag = 'x'\nzoom_level_count = { from = 'y' }\n", "a number" },
			{ "copied", "[[tags]]\ngroup = \"weapon_hud_interface\"\ntag = 'x'\ncrosshairs = 'y'\n", "is copied" },
			{ "many", "[[tags]]\ngroup = \"weapon_hud_interface\"\ntag = 'x'\n"
				"screen_effects = { from = 'y', elements = [1] }\n", "at most 1" },
			{ "copy_key", "[[tags]]\ngroup = \"weapon_hud_interface\"\ntag = 'x'\n"
				"crosshairs = { from = 'y', run = 'z' }\n", "not run" },
			{ "nameless", "[[tags]]\ngroup = \"weapon\"\nzoom_level_count = 1\n", "names its group and its tag" },
			{ "reference", "[[tags]]\ngroup = \"weapon\"\ntag = 'x'\nzoom_in_sound = 3\n", "a tag's name" },
			{ "nothing", "name = \"Nothing\"\n", "no effect and edits no tag" },
			{ "later", "format = 3\n[[tags]]\ngroup = \"weapon\"\ntag = 'x'\nzoom_level_count = 1\n", "later build" },
		};
		for (index = 0; index < (int)NUMBEROF(cases); index++)
			folder(cases[index].id, cases[index].text);
		port_mutators_scan();
		for (index = 0; index < (int)NUMBEROF(cases); index++)
		{
			CHECK(port_mutators_status(find(cases[index].id), NULL, 0) == _port_mutator_unplayable &&
				strstr(refusal(cases[index].id), cases[index].reason), "%s: %s", cases[index].id,
				refusal(cases[index].id));
		}
		CHECK(edits_of("group,field,range,enum,flags,copy,many,nothing", edits, sizeof(edits)) == 0,
			"a refused mutator's edits played");
	}
	else if (!strcmp(case_name, "bytes"))
	{
		static unsigned char changed[MAXIMUM_MUTATOR_TAG_EDIT_BYTES];
		long first;

		size = edits_of("accurized_ma5b", edits, sizeof(edits));
		/* (the first edit's size: its header, four texts and no elements) */
		first = 8;
		for (index = 0; index < 4; index++)
			first += (long)strlen((char const *)edits + first) + 1;
		CHECK(mutator_tags_play(edits, first) && mutator_tags.made == 1, "one edit");
		mutator_tags_undo();
		CHECK(!mutator_tags_play(edits, first - 1) && unchanged(&stock), "an edit cut short");
		CHECK(!mutator_tags_play(edits, size - 1) && unchanged(&stock), "the last edit cut short: the others made");
		memcpy(changed, edits, size);
		changed[0] = NUMBER_OF_MUTATOR_TAG_EDIT_KINDS;
		CHECK(!mutator_tags_play(changed, size) && unchanged(&stock), "a kind this build has not");
		memcpy(changed, edits, size);
		changed[3] = 1;
		CHECK(!mutator_tags_play(changed, size) && unchanged(&stock), "the unused byte set");
		memcpy(changed, edits, size);
		changed[1] = MAXIMUM_MUTATOR_TAG_COPY_ELEMENTS + 1;
		CHECK(!mutator_tags_play(changed, size) && unchanged(&stock), "too many elements");
		/* (an edit's text not ended within its most bytes) */
		memset(changed, 0, sizeof(changed));
		changed[0] = _mutator_tag_edit_number;
		memset(changed + 8, 'a', MUTATOR_TAG_GROUP_LENGTH + 1);
		CHECK(!mutator_tags_play(changed, sizeof(changed)) && unchanged(&stock), "a group not ended");
		/* (a value out of range on the wire, as a host of a fork could send: the
		first edit is zoom_level_count, of 0 to 8) */
		memcpy(changed, edits, size);
		CHECK(!strcmp((char const *)changed + 8 + strlen("weapon") + 1 + strlen((char const *)changed + 8 +
			strlen("weapon") + 1) + 1, "zoom_level_count"), "the first edit");
		{
			float value = 9.f;
			unsigned long bits;

			memcpy(&bits, &value, sizeof(bits));
			changed[4] = (unsigned char)bits;
			changed[5] = (unsigned char)(bits >> 8);
			changed[6] = (unsigned char)(bits >> 16);
			changed[7] = (unsigned char)(bits >> 24);
		}
		CHECK(!mutator_tags_play(changed, size) && unchanged(&stock), "a value out of range on the wire");
		CHECK(!mutators_tag_edit_check(edits, size, NULL, 0), "more than one edit checked as one");
		CHECK(mutators_tag_edit_check(edits, first, NULL, 0), "one edit refused");
	}
	else
	{
		CHECK(0, "no case %s", case_name);
	}
	return 0;
}
