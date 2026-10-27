/*
PORT_MUTATORS.C

The mutators (port_mutators.h): those built into the game, then the folders
of the data root's mutators folder in their names' alphabetical order, each
with a mutator.toml. A mutator is only data: it names effects the game has
built in (port/linux/game/mutators.c) and its parameters' values, and values
of tags it sets (port/linux/game/mutator_tags.c). Nothing in it runs, and the
game checks each key and value against the effects it has
(mutators_key_check), and each tag edit against the fields it can set
(mutators_tag_edit_check): a mutator with one it does not have, or a value
out of its range, cannot be played, and the Mods menu says why.

mutator.toml (all but effects or tags may be left out):

	name = "Big head"             # what the menu shows (else its folder's name)
	id = "big_head"               # what game.mutators names it by (else its folder's name)
	version = "1.0"
	authors = ["...", "..."]      # or one string
	description = "..."
	format = 1                    # the mutators' format it is made for

	[effects.big_head]            # an effect it turns on
	scale = 2.5                   # and its parameters (else their defaults)

	[[tags]]                      # a tag it edits
	group = "weapon"              # its group, by mutator_tags.c's name
	tag = 'weapons\pistol\pistol' # its name
	zoom_level_count = 1          # a field and its value (a number, or text)
	[tags.triggers.0]             # a block's element (by index, or name)
	rounds_between_tracers = 5
	crosshairs = { from = '...', elements = [1, 2], append = true }
	                              # a block's elements, copied from another tag's

Each edit is kept as the bytes mutator_tags.c reads (its header says what
they are).
The mutators on are game.mutators's (their ids, separated by commas), which
the Mods menu's MUTATORS list turns on and off. Where two mutators that are
on set one parameter, the one found later sets it. On a client the host's
are played instead (port/linux/game/network_mutators.c).
*/

#include "platform.h"
#include "port_config.h"
#include "port_mutators.h"
#include "posix.h"
#include "tomlc17.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- constants */

/* the mutators' format this build plays (2: tag edits) */
#define MUTATORS_FORMAT 2

#define MAXIMUM_MUTATORS 64
#define MAXIMUM_MUTATOR_FOLDER_LENGTH 64
/* the most effects and parameters one mutator sets */
#define MAXIMUM_KEYS_PER_MUTATOR 16
/* the largest mutator.toml read (a mutator is a few lines) */
#define MAXIMUM_MUTATOR_FILE_SIZE 65536
#define MAXIMUM_PATH_LENGTH 1024
#define MAXIMUM_REFUSAL_LENGTH 160
/* the most bytes of tag edits a game plays, and one mutator has
(mutators.h's MAXIMUM_MUTATOR_TAG_EDIT_BYTES) */
#define MAXIMUM_TAG_EDIT_BYTES 32768
/* a tag edit's group's, tag's, path's and text's most bytes, ended, and its
copy's most elements (mutator_tags.c's) */
#define TAG_EDIT_GROUP_LENGTH 32
#define TAG_EDIT_NAME_LENGTH 256
#define TAG_EDIT_PATH_LENGTH 64
#define TAG_EDIT_TEXT_LENGTH 256
#define MAXIMUM_TAG_EDIT_ELEMENTS 64

/* a tag edit's kind (mutators.h's) */
enum
{
	_tag_edit_number,
	_tag_edit_text,
	_tag_edit_copy,
};

/* ---------- structures */

struct port_mutator
{
	char folder[MAXIMUM_MUTATOR_FOLDER_LENGTH];
	char id[MAXIMUM_MUTATOR_FOLDER_LENGTH];
	char name[128];
	char version[32];
	char authors[128];
	char description[256];
	int built_in;
	int on;
	/* why it cannot be played, or empty */
	char refusal[MAXIMUM_REFUSAL_LENGTH];
	long key_count;
	char keys[MAXIMUM_KEYS_PER_MUTATOR][PORT_MUTATOR_KEY_LENGTH];
	double values[MAXIMUM_KEYS_PER_MUTATOR];
	/* its tag edits' bytes (allocated), how many bytes and edits, and how
	many tags they are of */
	unsigned char *tag_edits;
	long tag_edit_size;
	long tag_edit_count;
	long tag_count;
};

/* ---------- globals */

/* the mutators built into the game, read as a folder's would be */
static const struct
{
	const char *folder;
	const char *text;
} built_in_mutators[] =
{
	{ "big_head",
		"name = \"Big head\"\n"
		"id = \"big_head\"\n"
		"version = \"1\"\n"
		"description = \"Every biped's head, the players' and the AI's, is drawn and hit at two and a half times its size.\"\n"
		"format = 1\n"
		"\n"
		"[effects.big_head]\n"
		"scale = 2.5\n" },
	{ "accurized_ma5b",
		"name = \"Accurized MA5B\"\n"
		"id = \"accurized_ma5b\"\n"
		"version = \"1\"\n"
		"description = \"The assault rifle zooms 2x with the pistol's scope, its bullets spread from 0.05 to 3 "
		"degrees, carry 300 world units and ricochet off stone, metal, ice and energy shields.\"\n"
		"format = 2\n"
		"\n"
		"[[tags]]\n"
		"group = \"weapon\"\n"
		"tag = 'weapons\\assault rifle\\assault rifle'\n"
		"zoom_level_count = 1\n"
		"zoom_magnification_minimum = 2\n"
		"zoom_magnification_maximum = 2\n"
		"zoom_in_sound = 'sound\\sfx\\weapons\\sniper rifle\\sniper_2x_zoom'\n"
		"zoom_out_sound = 'sound\\sfx\\weapons\\sniper rifle\\sniper_10x_zoom'\n"
		"[tags.triggers.0]\n"
		"projectile_error_angle_lower_bound = 0.05\n"
		"projectile_error_angle_upper_bound = 3\n"
		"rounds_between_tracers = 5\n"
		"\n"
		"[[tags]]\n"
		"group = \"projectile\"\n"
		"tag = 'weapons\\assault rifle\\bullet'\n"
		"maximum_range = 300\n"
		"[tags.material_responses.stone]\n"
		"potential_response = \"reflect\"\n"
		"potential_skip_fraction = 0.9\n"
		"potential_angle_upper_bound = 60\n"
		"potential_effect = 'weapons\\assault rifle\\effects\\impact stone'\n"
		"[tags.material_responses.hollow_metal]\n"
		"potential_response = \"reflect\"\n"
		"potential_skip_fraction = 0.9\n"
		"potential_angle_upper_bound = 60\n"
		"potential_effect = 'weapons\\assault rifle\\effects\\impact metal hollow reflect'\n"
		"[tags.material_responses.thin_metal]\n"
		"potential_response = \"reflect\"\n"
		"potential_skip_fraction = 0.9\n"
		"potential_angle_upper_bound = 60\n"
		"potential_effect = 'weapons\\assault rifle\\effects\\impact metal thin reflect'\n"
		"[tags.material_responses.thick_metal]\n"
		"potential_response = \"reflect\"\n"
		"potential_skip_fraction = 0.9\n"
		"potential_angle_upper_bound = 60\n"
		"potential_effect = 'weapons\\assault rifle\\effects\\impact metal thick reflect'\n"
		"[tags.material_responses.ice]\n"
		"potential_response = \"reflect\"\n"
		"potential_skip_fraction = 0.9\n"
		"potential_angle_upper_bound = 60\n"
		"potential_effect = 'weapons\\assault rifle\\effects\\impact ice'\n"
		"[tags.material_responses.force_field]\n"
		"default_response = \"reflect\"\n"
		"[tags.material_responses.hunter_armor]\n"
		"default_response = \"reflect\"\n"
		"[tags.material_responses.engineer_force_field]\n"
		"default_response = \"reflect\"\n"
		"[tags.material_responses.cyborg_energy_shield]\n"
		"default_response = \"reflect\"\n"
		"[tags.material_responses.elite_energy_shield]\n"
		"default_response = \"reflect\"\n"
		"\n"
		"[[tags]]\n"
		"group = \"weapon_hud_interface\"\n"
		"tag = 'weapons\\assault rifle\\assault rifle'\n"
		"valid_crosshair_types = [\"aim\", \"zoom\"]\n"
		"crosshairs = { from = 'weapons\\pistol\\pistol', elements = [1, 2], append = true }\n"
		"screen_effects = { from = 'weapons\\pistol\\pistol' }\n" },
	{ "revolved_plasma_rifle",
		"name = \"Revolved plasma rifle\"\n"
		"id = \"revolved_plasma_rifle\"\n"
		"version = \"1\"\n"
		"description = \"The plasma rifle as Combat Revolved has it: a 2x zoom with the pistol's scope, 6 to 9 shots a "
		"second, a spread of at most 0.75 degrees, slower bolts of 13 damage that reach 45 world units, and less "
		"overheating and wear.\"\n"
		"format = 2\n"
		"\n"
		"[[tags]]\n"
		"group = \"weapon\"\n"
		"tag = 'weapons\\plasma rifle\\plasma rifle'\n"
		"heat_recovery_threshold = 0.15\n"
		"heat_loss_per_second = 0.36\n"
		"zoom_level_count = 1\n"
		"zoom_magnification_minimum = 2\n"
		"zoom_magnification_maximum = 2\n"
		"zoom_in_sound = 'sound\\sfx\\weapons\\sniper rifle\\sniper_2x_zoom'\n"
		"zoom_out_sound = 'sound\\sfx\\weapons\\sniper rifle\\sniper_10x_zoom'\n"
		"age_heat_recovery_penalty = 0\n"
		"age_rate_of_fire_penalty = 0\n"
		"age_misfire_start = 0.85\n"
		"age_misfire_chance = 0.25\n"
		"active_camo_ding = 0.25\n"
		"active_camo_regrowth_rate = 0.075\n"
		"[tags.triggers.0]\n"
		"initial_rate_of_fire = 6\n"
		"final_rate_of_fire = 9\n"
		"rate_of_fire_acceleration_time = 0.8\n"
		"rate_of_fire_deceleration_time = 0.6\n"
		"rounds_between_tracers = 0\n"
		"error_acceleration_time = 0.8\n"
		"error_deceleration_time = 0.6\n"
		"projectile_error_angle_upper_bound = 0.75\n"
		"heat_generated_per_round = 0.0861\n"
		"age_generated_per_round = 0.003\n"
		"\n"
		"[[tags]]\n"
		"group = \"projectile\"\n"
		"tag = 'weapons\\plasma rifle\\bolt'\n"
		"maximum_range = 45\n"
		"air_damage_range_lower_bound = 15\n"
		"air_damage_range_upper_bound = 45\n"
		"water_damage_range_lower_bound = 15\n"
		"water_damage_range_upper_bound = 45\n"
		"initial_velocity = 27\n"
		"final_velocity = 27\n"
		"[tags.material_responses.hunter_shield]\n"
		"default_response = \"disappear\"\n"
		"\n"
		"[[tags]]\n"
		"group = \"damage_effect\"\n"
		"tag = 'weapons\\plasma rifle\\bolt'\n"
		"damage_minimum = 13\n"
		"damage_lower_bound = 13\n"
		"damage_upper_bound = 13\n"
		"active_camouflage_damage = 1\n"
		"[tags.material_modifiers]\n"
		"dirt = 1\n"
		"sand = 1\n"
		"stone = 1\n"
		"snow = 1\n"
		"wood = 1\n"
		"hollow_metal = 0.5\n"
		"rubber = 1\n"
		"hunter_armor = 0.25\n"
		"hunter_skin = 1\n"
		"jackal_energy_shield = 2\n"
		"engineer_force_field = 2\n"
		"cyborg = 1\n"
		"monitor = 1\n"
		"plastic = 1\n"
		"water = 1\n"
		"leaves = 1\n"
		"ice = 1\n"
		"\n"
		"[[tags]]\n"
		"group = \"weapon_hud_interface\"\n"
		"tag = 'weapons\\plasma rifle\\plasma rifle'\n"
		"valid_crosshair_types = [\"aim\", \"zoom\", \"flash_total_battery\"]\n"
		"crosshairs = { from = 'weapons\\pistol\\pistol', elements = [1, 2], append = true }\n"
		"screen_effects = { from = 'weapons\\pistol\\pistol' }\n" },
};

static struct
{
	struct port_mutator mutators[MAXIMUM_MUTATORS];
	long count;
	unsigned long scans;
} port_mutators;

/* ---------- prototypes/MUTATORS.C (port/linux/game/mutators.h's) */

int mutators_key_check(const char *key, double value, char *reason, unsigned long size);
int mutators_tag_edit_check(const unsigned char *edit, long size, char *reason, unsigned long reason_size);

/* ---------- private code */

static char lower(char character)
{
	return character >= 'A' && character <= 'Z' ? (char)(character - 'A' + 'a') : character;
}

static int names_compare(const char *first, const char *second)
{
	for (; *first && lower(*first) == lower(*second); first++, second++)
	{
	}
	return (unsigned char)lower(*first) - (unsigned char)lower(*second);
}

/* text as mutator.toml has it, its control characters (but its line breaks,
if `lines`) spaces, its last character whole: what the menus show of it */
static void text_copy(char *destination, size_t size, const char *source, int lines)
{
	size_t length;
	char *text;

	snprintf(destination, size, "%s", source ? source : "");
	length = strlen(destination);
	/* (a UTF-8 character cut short at the end dropped) */
	while (length > 0 && ((unsigned char)destination[length - 1] & 0xC0) == 0x80)
		length--;
	if (length > 0 && (unsigned char)destination[length - 1] >= 0xC0)
	{
		unsigned char first = (unsigned char)destination[length - 1];
		size_t need = first >= 0xF0 ? 4 : first >= 0xE0 ? 3 : 2;

		if (strlen(destination) - (length - 1) < need)
			destination[length - 1] = 0;
	}
	for (text = destination; *text; text++)
	{
		if (((unsigned char)*text < 0x20 && !(lines && *text == '\n')) || *text == 0x7F)
			*text = ' ';
	}
}

/* whether an id can be named in game.mutators: printable, and no comma or
quote */
static int id_valid(const char *id)
{
	if (!id[0])
		return 0;
	for (; *id; id++)
	{
		if ((unsigned char)*id < 0x20 || *id == 0x7F || *id == ',' || *id == '"' || *id == '\\')
			return 0;
	}
	return 1;
}

static void refuse(struct port_mutator *mutator, const char *path, const char *reason)
{
	if (!mutator->refusal[0])
		snprintf(mutator->refusal, sizeof(mutator->refusal), "%s", reason);
	platform_log("mutators: %s: %s: it cannot be played", path, reason);
}

/* a string key of mutator.toml, or NULL (and refused when it is not one) */
static const char *toml_text(toml_datum_t table, const char *key, struct port_mutator *mutator, const char *path)
{
	toml_datum_t value = toml_get(table, key);
	char reason[MAXIMUM_REFUSAL_LENGTH];

	if (value.type == TOML_UNKNOWN)
		return NULL;
	if (value.type != TOML_STRING)
	{
		snprintf(reason, sizeof(reason), "line %d: %s is not text", value.lineno, key);
		refuse(mutator, path, reason);
		return NULL;
	}
	return value.u.s;
}

static void key_add(struct port_mutator *mutator, const char *path, const char *key, double value)
{
	char reason[MAXIMUM_REFUSAL_LENGTH];

	if (strlen(key) >= PORT_MUTATOR_KEY_LENGTH || !mutators_key_check(key, value, reason, sizeof(reason)))
	{
		if (strlen(key) >= PORT_MUTATOR_KEY_LENGTH)
			snprintf(reason, sizeof(reason), "this game has no effect or parameter %.40s...", key);
		refuse(mutator, path, reason);
		return;
	}
	if (mutator->key_count == MAXIMUM_KEYS_PER_MUTATOR)
	{
		snprintf(reason, sizeof(reason), "it sets more than %d effects and parameters", MAXIMUM_KEYS_PER_MUTATOR);
		refuse(mutator, path, reason);
		return;
	}
	snprintf(mutator->keys[mutator->key_count], PORT_MUTATOR_KEY_LENGTH, "%s", key);
	mutator->values[mutator->key_count++] = value;
}

/* [effects.<name>] and its parameters: each effect's key, then its
parameters' ("big_head", "big_head.scale") */
static void effects_read(struct port_mutator *mutator, toml_datum_t effects, const char *path)
{
	char reason[MAXIMUM_REFUSAL_LENGTH];
	int effect;

	if (effects.type != TOML_TABLE)
	{
		snprintf(reason, sizeof(reason), "line %d: effects is not a table of effects", effects.lineno);
		refuse(mutator, path, reason);
		return;
	}
	for (effect = 0; effect < effects.u.tab.size; effect++)
	{
		const char *name = effects.u.tab.key[effect];
		toml_datum_t parameters = effects.u.tab.value[effect];
		int parameter;

		if (parameters.type != TOML_TABLE)
		{
			snprintf(reason, sizeof(reason), "line %d: effects.%.40s is not a table of parameters", parameters.lineno,
				name);
			refuse(mutator, path, reason);
			return;
		}
		key_add(mutator, path, name, 1.0);
		for (parameter = 0; parameter < parameters.u.tab.size; parameter++)
		{
			toml_datum_t value = parameters.u.tab.value[parameter];
			char key[2 * PORT_MUTATOR_KEY_LENGTH];
			double number;

			if (value.type == TOML_FP64)
				number = value.u.fp64;
			else if (value.type == TOML_INT64)
				number = (double)value.u.int64;
			else if (value.type == TOML_BOOLEAN)
				number = value.u.boolean ? 1.0 : 0.0;
			else
			{
				snprintf(reason, sizeof(reason), "line %d: %.24s.%.24s is not a number", value.lineno, name,
					parameters.u.tab.key[parameter]);
				refuse(mutator, path, reason);
				return;
			}
			snprintf(key, sizeof(key), "%s.%s", name, parameters.u.tab.key[parameter]);
			key_add(mutator, path, key, number);
		}
	}
	if (!mutator->key_count)
		refuse(mutator, path, "it turns on no effect");
}

/* a tag edit: its bytes (mutator_tags.c's), checked, after the mutator's */
static void tag_edit_add(struct port_mutator *mutator, const char *path, int line, const char *group, const char *tag,
	const char *field, int kind, double number, const char *text, const long *elements, int element_count, int append)
{
	unsigned char edit[8 + TAG_EDIT_GROUP_LENGTH + TAG_EDIT_NAME_LENGTH + TAG_EDIT_PATH_LENGTH + TAG_EDIT_TEXT_LENGTH +
		2 * MAXIMUM_TAG_EDIT_ELEMENTS];
	char reason[MAXIMUM_REFUSAL_LENGTH];
	char check[MAXIMUM_REFUSAL_LENGTH - 16];
	const char *texts[4];
	float value = (float)number;
	unsigned long bits;
	unsigned char *grown;
	long size = 8;
	int index;

	texts[0] = group;
	texts[1] = tag;
	texts[2] = field;
	texts[3] = text ? text : "";
	if (strlen(group) >= TAG_EDIT_GROUP_LENGTH || strlen(tag) >= TAG_EDIT_NAME_LENGTH ||
		strlen(field) >= TAG_EDIT_PATH_LENGTH || strlen(texts[3]) >= TAG_EDIT_TEXT_LENGTH ||
		element_count > MAXIMUM_TAG_EDIT_ELEMENTS)
	{
		snprintf(reason, sizeof(reason), "line %d: a tag edit's name, field or value is too long", line);
		refuse(mutator, path, reason);
		return;
	}
	memcpy(&bits, &value, sizeof(bits));
	edit[0] = (unsigned char)kind;
	edit[1] = (unsigned char)element_count;
	edit[2] = (unsigned char)(append != 0);
	edit[3] = 0;
	edit[4] = (unsigned char)bits;
	edit[5] = (unsigned char)(bits >> 8);
	edit[6] = (unsigned char)(bits >> 16);
	edit[7] = (unsigned char)(bits >> 24);
	for (index = 0; index < 4; index++)
	{
		size_t length = strlen(texts[index]) + 1;

		memcpy(edit + size, texts[index], length);
		size += (long)length;
	}
	for (index = 0; index < element_count; index++)
	{
		edit[size++] = (unsigned char)elements[index];
		edit[size++] = (unsigned char)(elements[index] >> 8);
	}
	if (!mutators_tag_edit_check(edit, size, check, sizeof(check)))
	{
		snprintf(reason, sizeof(reason), "line %d: %s", line, check);
		refuse(mutator, path, reason);
		return;
	}
	if (mutator->tag_edit_size + size > MAXIMUM_TAG_EDIT_BYTES)
	{
		refuse(mutator, path, "its tag edits are more than 32 KB");
		return;
	}
	grown = realloc(mutator->tag_edits, (size_t)(mutator->tag_edit_size + size));
	if (!grown)
	{
		refuse(mutator, path, "its tag edits do not fit in memory");
		return;
	}
	mutator->tag_edits = grown;
	memcpy(mutator->tag_edits + mutator->tag_edit_size, edit, (size_t)size);
	mutator->tag_edit_size += size;
	mutator->tag_edit_count++;
}

/* a copy's table: { from = '<tag>', elements = [...], append = true } */
static void tag_copy_read(struct port_mutator *mutator, const char *path, const char *group, const char *tag,
	const char *field, toml_datum_t copy)
{
	long elements[MAXIMUM_TAG_EDIT_ELEMENTS];
	char reason[MAXIMUM_REFUSAL_LENGTH];
	toml_datum_t from = toml_get(copy, "from");
	toml_datum_t list = toml_get(copy, "elements");
	toml_datum_t append = toml_get(copy, "append");
	int element_count = 0;
	int index;

	for (index = 0; index < copy.u.tab.size; index++)
	{
		const char *key = copy.u.tab.key[index];

		if (strcmp(key, "from") && strcmp(key, "elements") && strcmp(key, "append"))
		{
			snprintf(reason, sizeof(reason), "line %d: a copy has from, elements and append, not %.40s", copy.lineno,
				key);
			refuse(mutator, path, reason);
			return;
		}
	}
	if (from.type != TOML_STRING || (list.type != TOML_UNKNOWN && list.type != TOML_ARRAY) ||
		(append.type != TOML_UNKNOWN && append.type != TOML_BOOLEAN))
	{
		snprintf(reason, sizeof(reason), "line %d: a copy's from is a tag's name, elements a list of numbers, append "
			"true or false", copy.lineno);
		refuse(mutator, path, reason);
		return;
	}
	if (list.type == TOML_ARRAY)
	{
		for (index = 0; index < list.u.arr.size; index++)
		{
			toml_datum_t element = list.u.arr.elem[index];

			if (element.type != TOML_INT64 || element.u.int64 < 0 || element.u.int64 > 32767 ||
				element_count == MAXIMUM_TAG_EDIT_ELEMENTS)
			{
				snprintf(reason, sizeof(reason), "line %d: a copy's elements are at most %d numbers from 0", copy.lineno,
					MAXIMUM_TAG_EDIT_ELEMENTS);
				refuse(mutator, path, reason);
				return;
			}
			elements[element_count++] = (long)element.u.int64;
		}
	}
	tag_edit_add(mutator, path, copy.lineno, group, tag, field, _tag_edit_copy, 0.0, from.u.s, elements, element_count,
		append.type == TOML_BOOLEAN && append.u.boolean);
}

/* a [[tags]] table's fields, those of its tables' as paths ("triggers.0.x") */
static void tag_fields_read(struct port_mutator *mutator, const char *path, const char *group, const char *tag,
	const char *prefix, toml_datum_t table)
{
	char reason[MAXIMUM_REFUSAL_LENGTH];
	int index;

	for (index = 0; index < table.u.tab.size; index++)
	{
		const char *key = table.u.tab.key[index];
		toml_datum_t value = table.u.tab.value[index];
		char field[TAG_EDIT_PATH_LENGTH + 1];

		if (!prefix[0] && (!strcmp(key, "group") || !strcmp(key, "tag")))
			continue;
		if (strlen(prefix) + strlen(key) + 2 > sizeof(field))
		{
			snprintf(reason, sizeof(reason), "line %d: the field %.40s... is not one a tag has", value.lineno, prefix);
			refuse(mutator, path, reason);
			return;
		}
		snprintf(field, sizeof(field), "%s%s%s", prefix, prefix[0] ? "." : "", key);
		if (value.type == TOML_TABLE)
		{
			if (toml_get(value, "from").type != TOML_UNKNOWN)
				tag_copy_read(mutator, path, group, tag, field, value);
			else
				tag_fields_read(mutator, path, group, tag, field, value);
		}
		else if (value.type == TOML_STRING)
		{
			tag_edit_add(mutator, path, value.lineno, group, tag, field, _tag_edit_text, 0.0, value.u.s, NULL, 0, 0);
		}
		else if (value.type == TOML_ARRAY)
		{
			/* (names, a field's flags: separated by commas) */
			char names[TAG_EDIT_TEXT_LENGTH] = "";
			int name;

			for (name = 0; name < value.u.arr.size; name++)
			{
				size_t length = strlen(names);

				if (value.u.arr.elem[name].type != TOML_STRING)
				{
					snprintf(reason, sizeof(reason), "line %d: %.40s is a list of names", value.lineno, field);
					refuse(mutator, path, reason);
					return;
				}
				snprintf(names + length, sizeof(names) - length, "%s%s", length ? "," : "", value.u.arr.elem[name].u.s);
			}
			tag_edit_add(mutator, path, value.lineno, group, tag, field, _tag_edit_text, 0.0, names, NULL, 0, 0);
		}
		else if (value.type == TOML_INT64 || value.type == TOML_FP64 || value.type == TOML_BOOLEAN)
		{
			double number = value.type == TOML_INT64 ? (double)value.u.int64 :
				value.type == TOML_FP64 ? value.u.fp64 : (value.u.boolean ? 1.0 : 0.0);

			tag_edit_add(mutator, path, value.lineno, group, tag, field, _tag_edit_number, number, NULL, NULL, 0, 0);
		}
		else
		{
			snprintf(reason, sizeof(reason), "line %d: %.40s is not a number, text or table", value.lineno, field);
			refuse(mutator, path, reason);
			return;
		}
	}
}

/* [[tags]]: each a tag's group and name, and the fields it sets */
static void tags_read(struct port_mutator *mutator, toml_datum_t tags, const char *path)
{
	char reason[MAXIMUM_REFUSAL_LENGTH];
	int index;

	if (tags.type != TOML_ARRAY)
	{
		snprintf(reason, sizeof(reason), "line %d: tags is not a list of tables ([[tags]])", tags.lineno);
		refuse(mutator, path, reason);
		return;
	}
	for (index = 0; index < tags.u.arr.size; index++)
	{
		toml_datum_t tag = tags.u.arr.elem[index];
		toml_datum_t group;
		toml_datum_t name;

		if (tag.type != TOML_TABLE)
		{
			snprintf(reason, sizeof(reason), "line %d: tags is not a list of tables ([[tags]])", tags.lineno);
			refuse(mutator, path, reason);
			return;
		}
		group = toml_get(tag, "group");
		name = toml_get(tag, "tag");
		if (group.type != TOML_STRING || name.type != TOML_STRING)
		{
			snprintf(reason, sizeof(reason), "line %d: a [[tags]] names its group and its tag", tag.lineno);
			refuse(mutator, path, reason);
			return;
		}
		tag_fields_read(mutator, path, group.u.s, name.u.s, "", tag);
		mutator->tag_count++;
	}
}

/* a mutator from its mutator.toml's text */
static void mutator_read(struct port_mutator *mutator, const char *folder, const char *text, size_t size,
	const char *path)
{
	static const char *const keys[] = { "name", "id", "version", "authors", "description", "format", "effects",
		"tags" };
	toml_result_t result;
	toml_datum_t value;
	char reason[MAXIMUM_REFUSAL_LENGTH];
	int index;

	text_copy(mutator->folder, sizeof(mutator->folder), folder, 0);
	text_copy(mutator->id, sizeof(mutator->id), folder, 0);
	text_copy(mutator->name, sizeof(mutator->name), folder, 0);
	result = toml_parse(text, (int)size);
	if (!result.ok)
	{
		snprintf(reason, sizeof(reason), "it is not TOML (%.100s)", result.errmsg);
		refuse(mutator, path, reason);
		toml_free(result);
		return;
	}
	if (toml_text(result.toptab, "name", mutator, path))
		text_copy(mutator->name, sizeof(mutator->name), toml_text(result.toptab, "name", mutator, path), 0);
	if (toml_text(result.toptab, "id", mutator, path))
		text_copy(mutator->id, sizeof(mutator->id), toml_text(result.toptab, "id", mutator, path), 0);
	text_copy(mutator->version, sizeof(mutator->version), toml_text(result.toptab, "version", mutator, path), 0);
	text_copy(mutator->description, sizeof(mutator->description),
		toml_text(result.toptab, "description", mutator, path), 1);
	value = toml_get(result.toptab, "authors");
	if (value.type == TOML_STRING)
	{
		text_copy(mutator->authors, sizeof(mutator->authors), value.u.s, 0);
	}
	else if (value.type == TOML_ARRAY)
	{
		for (index = 0; index < value.u.arr.size; index++)
		{
			size_t length = strlen(mutator->authors);

			if (value.u.arr.elem[index].type == TOML_STRING)
				snprintf(mutator->authors + length, sizeof(mutator->authors) - length, "%s%s", length ? ", " : "",
					value.u.arr.elem[index].u.s);
		}
		text_copy(mutator->authors, sizeof(mutator->authors), mutator->authors, 0);
	}
	value = toml_get(result.toptab, "format");
	if (value.type == TOML_INT64 && value.u.int64 > MUTATORS_FORMAT)
		refuse(mutator, path, "it is made for a later build");
	else if (value.type != TOML_UNKNOWN && value.type != TOML_INT64)
		refuse(mutator, path, "its format is not a number");
	if (!id_valid(mutator->id))
		refuse(mutator, path, "its id is empty or has a comma, quote or backslash");
	for (index = 0; index < result.toptab.u.tab.size; index++)
	{
		size_t key;

		for (key = 0; key < sizeof(keys) / sizeof(keys[0]); key++)
		{
			if (!strcmp(result.toptab.u.tab.key[index], keys[key]))
				break;
		}
		if (key == sizeof(keys) / sizeof(keys[0]))
		{
			/* (a key this build does not know may change what the mutator
			does: not played, as an effect it does not have is not) */
			snprintf(reason, sizeof(reason), "line %d: this game has no key %.40s",
				result.toptab.u.tab.value[index].lineno, result.toptab.u.tab.key[index]);
			refuse(mutator, path, reason);
		}
	}
	value = toml_get(result.toptab, "effects");
	if (value.type != TOML_UNKNOWN)
		effects_read(mutator, value, path);
	value = toml_get(result.toptab, "tags");
	if (value.type != TOML_UNKNOWN)
		tags_read(mutator, value, path);
	if (!mutator->key_count && !mutator->tag_edit_count)
		refuse(mutator, path, "it turns on no effect and edits no tag");
	toml_free(result);
}

/* (the mutator's tag edits are the list's after it) */
static void mutator_add(const struct port_mutator *mutator)
{
	long index;

	if (port_mutators.count == MAXIMUM_MUTATORS)
	{
		platform_log("mutators: more than %d: %s and those after it are left out", MAXIMUM_MUTATORS, mutator->folder);
		free(mutator->tag_edits);
		return;
	}
	port_mutators.mutators[port_mutators.count] = *mutator;
	for (index = 0; index < port_mutators.count; index++)
	{
		if (!names_compare(port_mutators.mutators[index].id, mutator->id))
		{
			refuse(&port_mutators.mutators[port_mutators.count], mutator->folder, "another mutator has its id");
			break;
		}
	}
	port_mutators.count++;
}

/* the folders of the mutators folder with a mutator.toml, by name */
static void mutators_find(const char *root)
{
	char names[MAXIMUM_MUTATORS][MAXIMUM_MUTATOR_FOLDER_LENGTH];
	long name_count = 0;
	void *directory = posix_directory_open(root);
	char name[256];
	long index;

	if (!directory)
		return;
	while (posix_directory_next(directory, name, sizeof(name)))
	{
		char path[MAXIMUM_PATH_LENGTH];
		struct posix_file_information information;
		long place;

		snprintf(path, sizeof(path), "%s/%s", root, name);
		if (name[0] == '.' || posix_stat(path, &information) != 0 || !(information.flags & _posix_file_is_directory))
			continue;
		if (strlen(name) >= MAXIMUM_MUTATOR_FOLDER_LENGTH)
		{
			platform_log("mutators: the folder name %s is longer than %d characters: it is not a mutator", name,
				MAXIMUM_MUTATOR_FOLDER_LENGTH - 1);
			continue;
		}
		if (name_count == MAXIMUM_MUTATORS)
		{
			platform_log("mutators: more than %d folders in %s: %s and those after it are left out", MAXIMUM_MUTATORS,
				root, name);
			break;
		}
		for (place = name_count; place > 0 && names_compare(names[place - 1], name) > 0; place--)
			memcpy(names[place], names[place - 1], sizeof(names[place]));
		snprintf(names[place], sizeof(names[place]), "%s", name);
		name_count++;
	}
	posix_directory_close(directory);
	for (index = 0; index < name_count; index++)
	{
		char path[MAXIMUM_PATH_LENGTH];
		struct posix_file_information information;
		struct port_mutator *mutator = calloc(1, sizeof(*mutator));
		char *text;
		size_t size = 0;

		if (!mutator)
			break;
		snprintf(path, sizeof(path), "%s/%s/mutator.toml", root, names[index]);
		if (posix_stat(path, &information) != 0)
		{
			platform_log("mutators: %s/%s has no mutator.toml: it is not a mutator", root, names[index]);
			free(mutator);
			continue;
		}
		if (information.size_high || information.size_low > MAXIMUM_MUTATOR_FILE_SIZE)
		{
			text_copy(mutator->folder, sizeof(mutator->folder), names[index], 0);
			text_copy(mutator->id, sizeof(mutator->id), names[index], 0);
			text_copy(mutator->name, sizeof(mutator->name), names[index], 0);
			refuse(mutator, path, "its mutator.toml is larger than 64 KB");
		}
		else if (!(text = config_file_read(path, &size)))
		{
			text_copy(mutator->folder, sizeof(mutator->folder), names[index], 0);
			text_copy(mutator->id, sizeof(mutator->id), names[index], 0);
			text_copy(mutator->name, sizeof(mutator->name), names[index], 0);
			refuse(mutator, path, "its mutator.toml cannot be read");
		}
		else
		{
			mutator_read(mutator, names[index], text, size, path);
			free(text);
		}
		mutator_add(mutator);
		free(mutator);
	}
}

/* whether game.mutators names `id` */
static int mutators_setting_names(const char *setting, const char *id)
{
	size_t length = strlen(id);

	while (*setting)
	{
		const char *end = strchr(setting, ',');
		const char *last;
		size_t item_length;

		if (!end)
			end = setting + strlen(setting);
		while (setting < end && (*setting == ' ' || *setting == '\t'))
			setting++;
		for (last = end; last > setting && (last[-1] == ' ' || last[-1] == '\t'); last--)
		{
		}
		item_length = (size_t)(last - setting);
		if (item_length == length)
		{
			size_t index;

			for (index = 0; index < length && lower(setting[index]) == lower(id[index]); index++)
			{
			}
			if (index == length)
				return 1;
		}
		setting = *end ? end + 1 : end;
	}
	return 0;
}

/* ---------- public code */

long port_mutators_scan(void)
{
	char root[MAXIMUM_PATH_LENGTH];
	long index;
	long on = 0;

	for (index = 0; index < port_mutators.count; index++)
		free(port_mutators.mutators[index].tag_edits);
	port_mutators.count = 0;
	port_mutators.scans++;
	for (index = 0; index < (long)(sizeof(built_in_mutators) / sizeof(built_in_mutators[0])); index++)
	{
		struct port_mutator *mutator = calloc(1, sizeof(*mutator));

		if (!mutator)
			break;
		mutator->built_in = 1;
		mutator_read(mutator, built_in_mutators[index].folder, built_in_mutators[index].text,
			strlen(built_in_mutators[index].text), built_in_mutators[index].folder);
		mutator_add(mutator);
		free(mutator);
	}
	snprintf(root, sizeof(root), "%s/mutators", platform_data_root());
	mutators_find(root);
	port_mutators_revert();
	for (index = 0; index < port_mutators.count; index++)
	{
		if (port_mutators.mutators[index].on && !port_mutators.mutators[index].refusal[0])
		{
			platform_log("mutators: %s (%s) is on", port_mutators.mutators[index].id, port_mutators.mutators[index].name);
			on++;
		}
	}
	return on;
}

long port_mutators_keys(char keys[][PORT_MUTATOR_KEY_LENGTH], double *values, long maximum)
{
	long count = 0;
	long index;

	if (!port_mutators.scans)
		port_mutators_scan();
	for (index = 0; index < port_mutators.count; index++)
	{
		const struct port_mutator *mutator = &port_mutators.mutators[index];
		long key;

		if (!mutator->on || mutator->refusal[0])
			continue;
		for (key = 0; key < mutator->key_count && count < maximum; key++)
		{
			memcpy(keys[count], mutator->keys[key], PORT_MUTATOR_KEY_LENGTH);
			values[count++] = mutator->values[key];
		}
	}
	return count;
}

long port_mutators_tag_edits(unsigned char *edits, long maximum)
{
	long size = 0;
	long index;

	if (!port_mutators.scans)
		port_mutators_scan();
	for (index = 0; index < port_mutators.count; index++)
	{
		const struct port_mutator *mutator = &port_mutators.mutators[index];

		if (!mutator->on || mutator->refusal[0] || !mutator->tag_edit_size)
			continue;
		if (size + mutator->tag_edit_size > maximum)
		{
			platform_log("mutators: the tag edits of those on are more than %ld bytes: %s's and those after are not "
				"played", maximum, mutator->id);
			break;
		}
		memcpy(edits + size, mutator->tag_edits, (size_t)mutator->tag_edit_size);
		size += mutator->tag_edit_size;
	}
	return size;
}

long port_mutators_count(void)
{
	return port_mutators.count;
}

const char *port_mutators_name(long index)
{
	return index >= 0 && index < port_mutators.count ? port_mutators.mutators[index].name : "";
}

int port_mutators_on(long index)
{
	return index >= 0 && index < port_mutators.count && port_mutators.mutators[index].on &&
		!port_mutators.mutators[index].refusal[0];
}

int port_mutators_status(long index, char *text, unsigned long size)
{
	if (text && size)
		text[0] = 0;
	if (index < 0 || index >= port_mutators.count || !port_mutators.mutators[index].refusal[0])
		return _port_mutator_fine;
	if (text)
		snprintf(text, size, "It cannot be played: %s.", port_mutators.mutators[index].refusal);
	return _port_mutator_unplayable;
}

int port_mutators_details(long index, char *text, unsigned long size)
{
	const struct port_mutator *mutator;
	size_t length;
	long key;

	if (!text || !size)
		return 0;
	text[0] = 0;
	if (index < 0 || index >= port_mutators.count)
		return 0;
	mutator = &port_mutators.mutators[index];
	if (mutator->version[0])
		snprintf(text, size, "Version %s\n", mutator->version);
	length = strlen(text);
	if (mutator->authors[0])
		snprintf(text + length, size - length, "By %s\n", mutator->authors);
	length = strlen(text);
	if (mutator->built_in)
		snprintf(text + length, size - length, "Built into the game\n");
	else
		snprintf(text + length, size - length, "Folder: %s\n", mutator->folder);
	for (key = 0; key < mutator->key_count; key++)
	{
		const char *dot = strchr(mutator->keys[key], '.');

		length = strlen(text);
		if (dot)
			snprintf(text + length, size - length, " %s %g", dot + 1, mutator->values[key]);
		else
			snprintf(text + length, size - length, "%s%s", key ? "\n" : "Effects: ", mutator->keys[key]);
	}
	length = strlen(text);
	if (mutator->tag_edit_count)
		snprintf(text + length, size - length, "%sTag edits: %ld, of %ld tags", mutator->key_count ? "\n" : "",
			mutator->tag_edit_count, mutator->tag_count);
	length = strlen(text);
	snprintf(text + length, size - length, "%sPlayed in the next game you host or play alone; a game you join "
		"plays its host's.\n", mutator->key_count || mutator->tag_edit_count ? "\n" : "");
	length = strlen(text);
	if (mutator->description[0])
		snprintf(text + length, size - length, "\n%s", mutator->description);
	return 1;
}

void port_mutators_turn(long index, int on)
{
	if (index >= 0 && index < port_mutators.count)
		port_mutators.mutators[index].on = on != 0;
}

int port_mutators_save(void)
{
	char setting[257] = "";
	long index;

	for (index = 0; index < port_mutators.count; index++)
	{
		const struct port_mutator *mutator = &port_mutators.mutators[index];
		size_t length = strlen(setting);

		if (!mutator->on || mutator->refusal[0])
			continue;
		if (length + strlen(mutator->id) + 2 >= sizeof(setting))
		{
			platform_log("mutators: too many on to keep in game.mutators: %s and those after it are not kept",
				mutator->id);
			break;
		}
		snprintf(setting + length, sizeof(setting) - length, "%s%s", length ? "," : "", mutator->id);
	}
	if (!config_write("game.mutators", setting))
	{
		platform_log("mutators: game.mutators cannot be written: the mutators on are kept until the game quits");
		return 0;
	}
	return 1;
}

void port_mutators_revert(void)
{
	const char *setting = config_string("game.mutators");
	long index;

	for (index = 0; index < port_mutators.count; index++)
		port_mutators.mutators[index].on = mutators_setting_names(setting, port_mutators.mutators[index].id);
}
