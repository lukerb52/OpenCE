/*
ADDONS.C

Add-ons (addons.h, docs/addons.md): the tags of a .cevehicle file
(addon_file.h), made by tools/cevehicle_build.c from a Custom Edition map,
added to the map a game plays when a mutator that is on names it
(port/linux/src/port_mutators.c).

A file is untrusted, as a map is: every section is checked to lie in it,
every pointer its relocations name to lie in its tag data, and then its
tags are laid out as a Custom Edition map's tags are (a header, a tag table,
the tag data) and checked by the validator as one's are (tag_validate.c),
apart from the map's. Only then are they made the map's: walked through
their groups' schemas (tag_walk.c), each tag a reference or tag index names
is given its index in the map's tag table, which the add-ons' tags join
after the map's own, before the menus' (menu_tags.c: their number is this
machine's alone, the add-ons' every machine's), their gbxmodels made this
build's models (custom_edition_geometry.c), the seats their riders sit in
added to the animation graphs of the map's bipeds that lack them, and their
vehicles placed in the scenario where the mutators say (port_mutators.c's
places). A tag of the add-on's that the map has (its group and name) is the
map's: the game finds a reference's tag by its name (tag_loaded), the map's
first. All of it is taken back before the map's tags go.

A bitmap's pixels and a sound's samples are read from the add-on's file
(addons_read, from cache_file_read), its bitmaps drawn as a Custom Edition
map's are (addons_bitmap_contains).
*/

/* ---------- headers */

#include "cseries.h"
#include "errors.h"
#include "tag_files/tag_groups.h"
#include "bitmaps/bitmap_group.h"
#include "models/model_definitions.h"
#include "objects/object_definitions.h"
#include "scenario/scenario_definitions.h"
#include "addon_file.h"
#include "addons.h"
#include "custom_edition_cache.h"
#include "tag_schema.h"
#include "tag_walk.h"

#include <stdlib.h>
#include <string.h>

/* ---------- constants */

enum
{
	MAXIMUM_ADDONS = 8,
	MAXIMUM_ADDON_PLACES = 64,
	/* the most riders' animation graphs given seats */
	MAXIMUM_SEAT_CHANGES = 32,

	BIPED_GROUP_TAG = 'bipd',
	VEHICLE_GROUP_TAG = 'vehi',
	ANIMATION_GRAPH_GROUP_TAG = 'antr',
	SOUND_GROUP_TAG = 'snd!',
	/* a Custom Edition map's models, and this build's (custom_edition_geometry.c
	makes them so) */
	GBXMODEL_GROUP_TAG = 'mod2',
	MODEL_GROUP_TAG = 'mode',
	TAG_HEADER_SIGNATURE = 'tags',

	/* an animation graph's (model_animation_definitions.h): its unit seats
	and animations, a seat's, a weapon class's and a weapon type's blocks */
	ANIMATION_GRAPH_UNIT_SEATS_OFFSET = 0xC,
	ANIMATION_GRAPH_ANIMATIONS_OFFSET = 0x74,
	ANIMATION_BYTES = 0xB4,
	UNIT_SEAT_BYTES = 0x64,
	UNIT_SEAT_ANIMATIONS_OFFSET = 0x40,
	UNIT_SEAT_WEAPON_CLASSES_OFFSET = 0x58,
	WEAPON_CLASS_BYTES = 0xBC,
	WEAPON_CLASS_ANIMATIONS_OFFSET = 0x98,
	WEAPON_CLASS_WEAPON_TYPES_OFFSET = 0xB0,
	WEAPON_TYPE_BYTES = 0x3C,
	WEAPON_TYPE_ANIMATIONS_OFFSET = 0x30,

	/* a sound's pitch ranges and their permutations, and where a permutation
	names its own tag (cache_file_formats.c) */
	SOUND_PITCH_RANGES_OFFSET = 0x98,
	SOUND_PITCH_RANGE_BYTES = 0x48,
	SOUND_PITCH_RANGE_PERMUTATIONS_OFFSET = 0x3C,
	SOUND_PERMUTATION_BYTES = 0x7C,
	SOUND_PERMUTATION_CACHE_TAG_INDEX_OFFSET = 0x34,
	SOUND_PERMUTATION_RUNTIME_TAG_INDEX_OFFSET = 0x3C,

	/* a bitmap's bitmaps (bitmap_group.h's bitmap_group) */
	BITMAP_GROUP_BITMAPS_OFFSET = 0x60,

	/* a scenario's vehicles, its vehicle palette (scenario_definitions.h), and
	a vehicle placement (tag_schema_scenario.c's unit_placement) */
	VEHICLE_PLACEMENT_BYTES = 0x78,
	VEHICLE_PALETTE_ENTRY_BYTES = 0x30,
	MAXIMUM_VEHICLE_PLACEMENTS = 0x50,
	MAXIMUM_VEHICLE_PALETTE_ENTRIES = 0x50,
};

/* ---------- structures */

/* the game's tag table's entry (cache_files.c's cache_file_tag_instance) */
struct addon_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

/* the tags' header as a Custom Edition map has it, for the validator
(tag_validate.c's tag_validate_custom_edition_header) */
struct addon_tag_header
{
	struct addon_tag_instance *instances;
	long scenario_tag_index;
	unsigned long checksum;
	long tag_count;
	long model_part_count;
	long model_vertex_data_offset;
	long model_part_count_again;
	long model_index_data_offset;
	long model_data_size;
	unsigned long signature;
};

typedef char verify_addon_tag_instance_size[sizeof(struct addon_tag_instance) == 0x20 ? 1 : -1];
typedef char verify_addon_tag_header_size[sizeof(struct addon_tag_header) == 0x28 ? 1 : -1];

struct addon
{
	char name[64];
	/* the file, whole (its resources and model data are read from it) */
	byte *file;
	unsigned long file_size;
	struct addon_file_header header;
	/* its tags laid out as a Custom Edition map's: the header, the table,
	then the tag data */
	byte *tags;
	unsigned long tags_size;
	struct addon_tag_header *tag_header;
	byte *tag_data;
	long tag_count;
	/* its tags' indices in the map's table, and the handles a reference to
	each is given (an add-on's, or the map's tag of its name) */
	long first_index;
	long *handles;
	/* the map's tags its imports name */
	long *imports;
	/* its models' geometry (custom_edition_geometry.c) */
	void *geometry;
};

/* an animation graph given a seat, and the unit seats it had */
struct addon_seat_change
{
	struct tag_block *seats;
	struct tag_block original;
	byte *elements;
	/* the seat's copies of its blocks, made for this graph, and the blocks
of the add-on's tag data they were copied from */
	byte *copies[64];
	byte const *originals[64];
	long sizes[64];
	short copy_count;
};

/* a field of an add-on's tag that names a tag: where it is, and whether it
is a reference or a tag index (_tag_schema_reference, _tag_schema_tag_index) */
struct addon_field
{
	void *address;
	short type;
};

/* the walk of an add-on's tags once checked (addon_fields_walk): the
fields that name tags, by where they are */
struct addon_walk
{
	struct addon *addon;
	struct addon_field *fields;
	long field_count;
	long field_capacity;
	boolean failed;
};

/* ---------- globals */

static struct
{
	struct addon addons[MAXIMUM_ADDONS];
	short count;
	/* the map's table, and the table with the add-ons' tags after it */
	struct addon_tag_instance *original_instances;
	long original_count;
	struct addon_tag_instance *instances;
	long count_with_addons;
	/* the riders' seats added */
	struct addon_seat_change seat_changes[MAXIMUM_SEAT_CHANGES];
	short seat_change_count;
	/* the scenario's vehicles and vehicle palette as they were, and the
	blocks that took their place */
	struct tag_block *vehicles;
	struct tag_block original_vehicles;
	byte *placed_vehicles;
	struct tag_block *vehicle_palette;
	struct tag_block original_vehicle_palette;
	byte *placed_vehicle_palette;
} addons_globals;

/* ---------- prototypes/the platform's (port/linux/src/port_mutators.c) */

struct port_mutator_place
{
	char map[64];
	char vehicle[256];
	float position[3];
	float facing;
};

long port_mutators_addon_count(void);
unsigned long port_mutators_addon_size(long index, char *name, unsigned long name_size);
int port_mutators_addon_read(long index, void *buffer, unsigned long size);
long port_mutators_places(struct port_mutator_place *places, long maximum);

/* ---------- prototypes/CACHE_FILES.C */

void *cache_files_tag_instances(long *count);
void cache_files_set_tag_instances(void *instances, long count);

/* ---------- private code */

static uint32_t read_u32(
	byte const *bytes)
{
	uint32_t value;

	csmemcpy(&value, bytes, sizeof(value));

	return value;
}

static void write_u32(
	byte *bytes,
	uint32_t value)
{
	csmemcpy(bytes, &value, sizeof(value));

	return;
}

/* whether count elements of element_size bytes at offset lie in size bytes */
static boolean within(
	uint32_t offset,
	uint32_t count,
	uint32_t element_size,
	unsigned long size)
{
	return offset <= size && (!element_size || count <= (size - offset) / element_size);
}

/* a string at offset in the tag data, ended within it, or NULL */
static char const *tag_data_string(
	struct addon const *addon,
	uint32_t offset)
{
	unsigned long size = addon->header.tag_data.count;

	if (offset >= size || !memchr(addon->tag_data + offset, 0, size - offset))
		return NULL;

	return (char const *)addon->tag_data + offset;
}

/* the group a tag of group_tag is in this build's table (a gbxmodel is a
model) */
static long table_group(
	long group_tag)
{
	return group_tag == GBXMODEL_GROUP_TAG ? MODEL_GROUP_TAG : group_tag;
}

/* the map's tag of a group and name, or NONE (as tag_loaded finds it) */
static long map_tag_find(
	long group_tag,
	char const *name)
{
	long index;

	for (index = 0; index < addons_globals.original_count; index++)
	{
		struct addon_tag_instance const *instance = &addons_globals.original_instances[index];

		if (instance->group_tag == table_group(group_tag) && !_stricmp(instance->name, name))
			return instance->tag_index;
	}

	return NONE;
}

/* the add-on's file read and its sections checked: FALSE (said) if it
cannot be played */
static boolean addon_file_read(
	struct addon *addon,
	long index)
{
	struct addon_file_header const *header = &addon->header;
	unsigned long size = port_mutators_addon_size(index, addon->name, sizeof(addon->name));

	if (size < sizeof(addon->header))
	{
		error(_error_silent, "add-ons: %s's file cannot be read", addon->name);
		return FALSE;
	}
	addon->file = malloc(size);
	if (!addon->file)
	{
		error(_error_silent, "add-ons: out of memory for %s's file (%lu bytes)", addon->name, size);
		return FALSE;
	}
	addon->file_size = size;
	if (!port_mutators_addon_read(index, addon->file, size))
	{
		error(_error_silent, "add-ons: %s's file cannot be read", addon->name);
		return FALSE;
	}
	csmemcpy(&addon->header, addon->file, sizeof(addon->header));
	if (header->signature != ADDON_FILE_SIGNATURE || header->kind != _addon_file_kind_vehicle ||
		header->file_size != size)
	{
		error(_error_silent, "add-ons: %s's file is not a .cevehicle", addon->name);
		return FALSE;
	}
	if (header->version != ADDON_FILE_VERSION)
	{
		error(_error_silent, "add-ons: %s's file is of version %lu, this build plays %d: build it again",
			addon->name, (unsigned long)header->version, ADDON_FILE_VERSION);
		return FALSE;
	}
	if (!within(header->tags.offset, header->tags.count, sizeof(struct addon_file_tag), size) ||
		!within(header->imports.offset, header->imports.count, sizeof(struct addon_file_import), size) ||
		!within(header->tag_data.offset, header->tag_data.count, 1, size) ||
		!within(header->relocations.offset, header->relocations.count, sizeof(uint32_t), size) ||
		!within(header->import_fields.offset, header->import_fields.count, sizeof(struct addon_file_import_field),
			size) ||
		!within(header->resources.offset, header->resources.count, 1, size) ||
		!within(header->model_data.offset, header->model_data.count, 1, size) ||
		!within(header->seats.offset, header->seats.count, sizeof(struct addon_file_seat), size) ||
		!within(header->seat_animations.offset, header->seat_animations.count,
			sizeof(struct addon_file_seat_animation), size) ||
		header->tags.count == 0 || header->tags.count > UNSIGNED_SHORT_MAX / 2 ||
		header->root_tag >= header->tags.count ||
		header->tag_data.count > TAG_VALIDATE_MAXIMUM_TAG_CACHE_SIZE / 2 ||
		header->model_index_data_offset > header->model_data.count)
	{
		error(_error_silent, "add-ons: %s's file is damaged (its sections are outside it)", addon->name);
		return FALSE;
	}
	addon->tag_count = (long)header->tags.count;

	return TRUE;
}

/* the add-on's tags laid out as a Custom Edition map's, their pointers made
this process's, and checked: FALSE (said) if they cannot be played */
static boolean addon_tags_check(
	struct addon *addon)
{
	struct addon_file_header const *header = &addon->header;
	struct addon_file_tag const *tags = (struct addon_file_tag const *)(addon->file + header->tags.offset);
	uint32_t const *relocations = (uint32_t const *)(addon->file + header->relocations.offset);
	unsigned long table_size = sizeof(struct addon_tag_header) + addon->tag_count * sizeof(struct addon_tag_instance);
	unsigned long data_size = header->tag_data.count;
	struct tag_validate_file_range resources;
	long index;

	addon->tags_size = table_size + data_size;
	addon->tags = malloc(addon->tags_size + 1);
	if (!addon->tags)
	{
		error(_error_silent, "add-ons: out of memory for %s's tags", addon->name);
		return FALSE;
	}
	csmemset(addon->tags, 0, table_size);
	addon->tag_header = (struct addon_tag_header *)addon->tags;
	addon->tag_data = addon->tags + table_size;
	csmemcpy(addon->tag_data, addon->file + header->tag_data.offset, data_size);

	/* every pointer: an offset in the tag data, made its address */
	for (index = 0; index < (long)header->relocations.count; index++)
	{
		uint32_t offset;
		uint32_t value;

		csmemcpy(&offset, &relocations[index], sizeof(offset));
		if (offset > data_size || data_size - offset < sizeof(uint32_t) ||
			(value = read_u32(addon->tag_data + offset)) >= data_size)
		{
			error(_error_silent, "add-ons: %s's file is damaged (a pointer outside its tags)", addon->name);
			return FALSE;
		}
		write_u32(addon->tag_data + offset, (uint32_t)(unsigned long)(addon->tag_data + value));
	}

	/* the table: each tag's index, groups, name and root */
	addon->tag_header->instances = (struct addon_tag_instance *)(addon->tags + sizeof(struct addon_tag_header));
	addon->tag_header->scenario_tag_index = NONE;
	addon->tag_header->tag_count = addon->tag_count;
	addon->tag_header->signature = TAG_HEADER_SIGNATURE;
	for (index = 0; index < addon->tag_count; index++)
	{
		struct addon_tag_instance *instance = &addon->tag_header->instances[index];
		char const *name = tag_data_string(addon, tags[index].name_offset);

		if (!name || tags[index].root_offset >= data_size)
		{
			error(_error_silent, "add-ons: %s's file is damaged (a tag outside its tags)", addon->name);
			return FALSE;
		}
		instance->group_tag = (long)tags[index].group_tag;
		instance->parent_group_tags[0] = (long)tags[index].parent_group_tags[0];
		instance->parent_group_tags[1] = (long)tags[index].parent_group_tags[1];
		instance->tag_index = index;
		instance->name = (char *)name;
		instance->base_address = addon->tag_data + tags[index].root_offset;
	}

	/* (its pixels and samples are at offsets in its resources) */
	resources.offset = 0;
	resources.size = header->resources.count;
	if (!tag_validate_addon_tags(addon->tags, (long)addon->tags_size, &resources, addon->name))
	{
		error(_error_silent, "add-ons: %s's tags are refused by the validator (above)", addon->name);
		return FALSE;
	}
	if (tag_validate_corrections())
		error(_error_silent, "add-ons: %s's tags needed %ld corrections (above)", addon->name, tag_validate_corrections());

	return TRUE;
}

static int field_compare(
	void const *a,
	void const *b)
{
	unsigned long first = (unsigned long)((struct addon_field const *)a)->address;
	unsigned long second = (unsigned long)((struct addon_field const *)b)->address;

	return first < second ? -1 : first > second;
}

/* each reference and tag index naming one of the add-on's tags given that
tag's handle, and its place kept for the imports (addon_fields_walk) */
static void field_visit(
	void *context,
	struct tag_schema_field const *field,
	void *address)
{
	struct addon_walk *walk = context;
	struct addon *addon = walk->addon;
	long *index_field;

	if (field->type != _tag_schema_reference && field->type != _tag_schema_tag_index)
		return;
	if (walk->failed)
		return;
	if (walk->field_count == walk->field_capacity)
	{
		long capacity = walk->field_capacity ? walk->field_capacity * 2 : 1024;
		struct addon_field *fields = malloc(capacity * sizeof(*fields));

		if (!fields)
		{
			walk->failed = TRUE;
			return;
		}
		if (walk->field_count)
		{
			csmemcpy(fields, walk->fields, walk->field_count * sizeof(*fields));
			free(walk->fields);
		}
		walk->fields = fields;
		walk->field_capacity = capacity;
	}
	walk->fields[walk->field_count].address = address;
	walk->fields[walk->field_count++].type = field->type;
	if (field->type == _tag_schema_reference)
	{
		struct tag_reference *reference = address;

		reference->group_tag = table_group(reference->group_tag);
		index_field = &reference->index;
	}
	else
	{
		index_field = address;
	}
	/* (the validator made each one none or one of the add-on's tags) */
	if (*index_field != NONE)
		*index_field = addon->handles[DATUM_INDEX_TO_ABSOLUTE_INDEX(*index_field)];

	return;
}

/* the add-on's tags' references and tag indices made the map's, and its
import fields given the map's tags they name: FALSE (said) if one names a
field that is not a reference's or tag index's */
static boolean addon_fields_walk(
	struct addon *addon)
{
	struct addon_file_import_field const *imports =
		(struct addon_file_import_field const *)(addon->file + addon->header.import_fields.offset);
	struct addon_walk walk;
	long index;
	boolean walked = TRUE;

	csmemset(&walk, 0, sizeof(walk));
	walk.addon = addon;
	for (index = 0; index < addon->tag_count && walked && !walk.failed; index++)
	{
		struct addon_tag_instance const *instance = &addon->tag_header->instances[index];

		walked = tag_walk(instance->base_address, (unsigned long)instance->group_tag, TRUE, field_visit, &walk);
	}
	if (!walked || walk.failed)
	{
		error(_error_silent, "add-ons: %s's tags cannot be walked", addon->name);
		walked = FALSE;
	}
	if (walked && walk.field_count)
		qsort(walk.fields, walk.field_count, sizeof(*walk.fields), field_compare);
	for (index = 0; walked && index < (long)addon->header.import_fields.count; index++)
	{
		struct addon_file_import_field import;
		boolean reference;
		uint32_t import_index;
		struct addon_field key;
		struct addon_field const *found;
		void *address;
		long *index_field;

		csmemcpy(&import, &imports[index], sizeof(import));
		reference = (import.import & ADDON_FILE_IMPORT_REFERENCE_BIT) != 0;
		import_index = import.import & ~ADDON_FILE_IMPORT_REFERENCE_BIT;
		address = addon->tag_data + import.offset;
		key.address = address;
		key.type = 0;
		found = import.offset < addon->header.tag_data.count && import_index < addon->header.imports.count &&
			walk.field_count ? bsearch(&key, walk.fields, walk.field_count, sizeof(*walk.fields), field_compare) : NULL;
		/* (a reference's field named as a reference, a tag index's as one) */
		if (found && found->type != (reference ? _tag_schema_reference : _tag_schema_tag_index))
			found = NULL;
		if (!found)
		{
			error(_error_silent, "add-ons: %s's file is damaged (an import field that names no field)", addon->name);
			walked = FALSE;
			break;
		}
		index_field = reference ? &((struct tag_reference *)address)->index : (long *)address;
		if (*index_field != NONE)
		{
			error(_error_silent, "add-ons: %s's file is damaged (an import field that names a tag of its own)",
				addon->name);
			walked = FALSE;
			break;
		}
		*index_field = addon->imports[import_index];
	}
	if (walk.fields)
		free(walk.fields);

	return walked;
}

/* the map's tags the add-on's imports name: FALSE (said) if the map lacks one */
static boolean addon_imports_find(
	struct addon *addon)
{
	struct addon_file_import const *imports =
		(struct addon_file_import const *)(addon->file + addon->header.imports.offset);
	long index;

	addon->imports = malloc((addon->header.imports.count + 1) * sizeof(*addon->imports));
	if (!addon->imports)
		return FALSE;
	for (index = 0; index < (long)addon->header.imports.count; index++)
	{
		char const *name = tag_data_string(addon, imports[index].name_offset);
		long tag_index = name ? map_tag_find((long)imports[index].group_tag, name) : NONE;
		char group[5];

		if (tag_index == NONE)
		{
			group[0] = (char)(imports[index].group_tag >> 24);
			group[1] = (char)(imports[index].group_tag >> 16);
			group[2] = (char)(imports[index].group_tag >> 8);
			group[3] = (char)imports[index].group_tag;
			group[4] = 0;
			error(_error_silent, "add-ons: %s needs the map's %s '%s', which this map has not", addon->name, group,
				name ? name : "");
			return FALSE;
		}
		addon->imports[index] = tag_index;
	}

	return TRUE;
}

/* the add-on's tags' places in the map's table (after first_index, their
salts after first_salt), and the handle a reference to each is given */
static boolean addon_handles_make(
	struct addon *addon,
	long first_index,
	long first_salt)
{
	long index;

	addon->first_index = first_index;
	addon->handles = malloc(addon->tag_count * sizeof(*addon->handles));
	if (!addon->handles)
		return FALSE;
	for (index = 0; index < addon->tag_count; index++)
	{
		struct addon_tag_instance const *instance = &addon->tag_header->instances[index];
		long existing = map_tag_find(instance->group_tag, instance->name);

		addon->handles[index] = existing != NONE ? existing :
			DATUM_INDEX_NEW(first_index + index, (first_salt + index) & UNSIGNED_SHORT_MAX);
	}

	return TRUE;
}

/* the add-on's bitmaps and sounds naming their own tags, as the game reads
them (cache_file_read) */
static void addon_resources_name(
	struct addon *addon)
{
	long index;

	for (index = 0; index < addon->tag_count; index++)
	{
		struct addon_tag_instance const *instance = &addon->tag_header->instances[index];
		long handle = addon->handles[index];
		byte *root = instance->base_address;
		long element;

		/* (a tag the map has is the map's: its own is read by none) */
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(handle) != addon->first_index + index)
			continue;
		if (instance->group_tag == BITMAP_GROUP_TAG)
		{
			struct tag_block const *bitmaps = (struct tag_block const *)(root + BITMAP_GROUP_BITMAPS_OFFSET);

			for (element = 0; element < bitmaps->count; element++)
				((struct bitmap_data *)bitmaps->address)[element].tag_index = handle;
		}
		if (instance->group_tag == SOUND_GROUP_TAG)
		{
			struct tag_block const *pitch_ranges = (struct tag_block const *)(root + SOUND_PITCH_RANGES_OFFSET);

			for (element = 0; element < pitch_ranges->count; element++)
			{
				struct tag_block const *permutations = (struct tag_block const *)((byte *)pitch_ranges->address +
					element * SOUND_PITCH_RANGE_BYTES + SOUND_PITCH_RANGE_PERMUTATIONS_OFFSET);
				long permutation;

				for (permutation = 0; permutation < permutations->count; permutation++)
				{
					byte *bytes = (byte *)permutations->address + permutation * SOUND_PERMUTATION_BYTES;

					write_u32(bytes + SOUND_PERMUTATION_CACHE_TAG_INDEX_OFFSET, (uint32_t)handle);
					write_u32(bytes + SOUND_PERMUTATION_RUNTIME_TAG_INDEX_OFFSET, (uint32_t)handle);
				}
			}
		}
	}

	return;
}

/* the add-on's gbxmodels made this build's models: FALSE (said) if one
cannot be */
static boolean addon_models_convert(
	struct addon *addon)
{
	struct model **models = malloc((addon->tag_count + 1) * sizeof(*models));
	char const **names = malloc((addon->tag_count + 1) * sizeof(*names));
	long count = 0;
	long index;
	boolean converted = TRUE;

	if (!models || !names)
	{
		if (models)
			free(models);
		if (names)
			free(names);
		return FALSE;
	}
	for (index = 0; index < addon->tag_count; index++)
	{
		struct addon_tag_instance *instance = &addon->tag_header->instances[index];

		if (instance->group_tag == GBXMODEL_GROUP_TAG)
		{
			models[count] = instance->base_address;
			names[count++] = instance->name;
		}
	}
	if (count)
	{
		addon->geometry = custom_edition_addon_models_convert(models, names, count,
			addon->file + addon->header.model_data.offset, addon->header.model_data.count,
			addon->header.model_index_data_offset);
		converted = addon->geometry != NULL;
	}
	free(models);
	free(names);

	return converted;
}

static byte *seat_block_copy(
	struct addon_seat_change *change,
	struct tag_block *block,
	long element_size)
{
	byte *copy;

	if (block->count <= 0 || change->copy_count == NUMBEROF(change->copies))
		return NULL;
	copy = malloc(block->count * element_size);
	if (!copy)
		return NULL;
	csmemcpy(copy, block->address, block->count * element_size);
	change->copies[change->copy_count] = copy;
	change->originals[change->copy_count] = block->address;
	change->sizes[change->copy_count++] = block->count * element_size;
	block->address = copy;

	return copy;
}

/* the index in the graph of the animation named, or NONE */
static short graph_animation_find(
	byte const *graph,
	char const *name)
{
	struct tag_block const *animations = (struct tag_block const *)(graph + ANIMATION_GRAPH_ANIMATIONS_OFFSET);
	long index;

	for (index = 0; index < animations->count && index < SHORT_MAX; index++)
	{
		if (!strncmp((char const *)animations->address + index * ANIMATION_BYTES, name, ADDON_FILE_STRING_BYTES))
			return (short)index;
	}

	return NONE;
}

/* the add-on's seat added to the graph that lacks it, its animations the
graph's of their names: FALSE (said) if the graph lacks one */
static boolean addon_seat_add(
	struct addon *addon,
	struct addon_file_seat const *seat,
	byte *graph,
	char const *graph_name)
{
	struct tag_block *seats = (struct tag_block *)(graph + ANIMATION_GRAPH_UNIT_SEATS_OFFSET);
	struct addon_file_seat_animation const *animations =
		(struct addon_file_seat_animation const *)(addon->file + addon->header.seat_animations.offset);
	struct addon_seat_change *change;
	byte *element;
	long index;

	if (addons_globals.seat_change_count == MAXIMUM_SEAT_CHANGES || seats->count >= SHORT_MAX - 1)
		return FALSE;
	change = &addons_globals.seat_changes[addons_globals.seat_change_count];
	csmemset(change, 0, sizeof(*change));
	change->elements = malloc((seats->count + 1) * UNIT_SEAT_BYTES);
	if (!change->elements)
		return FALSE;
	csmemcpy(change->elements, seats->address, seats->count * UNIT_SEAT_BYTES);
	element = change->elements + seats->count * UNIT_SEAT_BYTES;
	csmemcpy(element, addon->tag_data + seat->element_offset, UNIT_SEAT_BYTES);

	/* (its blocks of animation indices are the graph's own: copied, with
	its weapon classes and types, which point at them) */
	{
		struct tag_block *classes = (struct tag_block *)(element + UNIT_SEAT_WEAPON_CLASSES_OFFSET);
		byte *class_elements;

		seat_block_copy(change, (struct tag_block *)(element + UNIT_SEAT_ANIMATIONS_OFFSET), sizeof(short));
		class_elements = seat_block_copy(change, classes, WEAPON_CLASS_BYTES);
		for (index = 0; class_elements && index < classes->count; index++)
		{
			byte *class_element = class_elements + index * WEAPON_CLASS_BYTES;
			struct tag_block *types = (struct tag_block *)(class_element + WEAPON_CLASS_WEAPON_TYPES_OFFSET);
			byte *type_elements;
			long type_index;

			seat_block_copy(change, (struct tag_block *)(class_element + WEAPON_CLASS_ANIMATIONS_OFFSET),
				sizeof(short));
			type_elements = seat_block_copy(change, types, WEAPON_TYPE_BYTES);
			for (type_index = 0; type_elements && type_index < types->count; type_index++)
			{
				seat_block_copy(change, (struct tag_block *)(type_elements + type_index * WEAPON_TYPE_BYTES +
					WEAPON_TYPE_ANIMATIONS_OFFSET), sizeof(short));
			}
		}
	}

	/* each animation the seat names, by its name in this graph, written in
	the copy of the block its index is in */
	for (index = 0; index < (long)seat->animation_count; index++)
	{
		struct addon_file_seat_animation animation;
		byte const *source;
		byte *field = NULL;
		short graph_index;
		short copy;

		csmemcpy(&animation, &animations[seat->first_animation + index], sizeof(animation));
		animation.name[ADDON_FILE_STRING_BYTES - 1] = 0;
		source = addon->tag_data + animation.offset;
		for (copy = 0; copy < change->copy_count && !field; copy++)
		{
			if (animation.offset < addon->header.tag_data.count && source >= change->originals[copy] &&
				source + sizeof(short) <= change->originals[copy] + change->sizes[copy])
			{
				field = change->copies[copy] + (source - change->originals[copy]);
			}
		}
		graph_index = graph_animation_find(graph, animation.name);
		if (!field || graph_index == NONE)
		{
			error(_error_silent, "add-ons: %s's seat '%s' needs the animation '%s', which '%s' has not", addon->name,
				seat->label, animation.name, graph_name);
			for (copy = 0; copy < change->copy_count; copy++)
				free(change->copies[copy]);
			free(change->elements);
			return FALSE;
		}
		csmemcpy(field, &graph_index, sizeof(graph_index));
	}

	change->seats = seats;
	change->original = *seats;
	seats->address = change->elements;
	seats->count++;
	addons_globals.seat_change_count++;
	error(_error_silent, "add-ons: %s's seat '%s' added to '%s' (%lu animations)", addon->name, seat->label, graph_name,
		(unsigned long)seat->animation_count);

	return TRUE;
}

/* whether the graph has a seat of the label */
static boolean graph_has_seat(
	byte const *graph,
	char const *label)
{
	struct tag_block const *seats = (struct tag_block const *)(graph + ANIMATION_GRAPH_UNIT_SEATS_OFFSET);
	long index;

	for (index = 0; index < seats->count; index++)
	{
		if (!strncmp((char const *)seats->address + index * UNIT_SEAT_BYTES, label, ADDON_FILE_STRING_BYTES))
			return TRUE;
	}

	return FALSE;
}

/* the add-on's seats added to the animation graphs of the map's bipeds that
lack them: FALSE (said) if one cannot be */
static boolean addon_seats_add(
	struct addon *addon)
{
	struct addon_file_seat const *seats = (struct addon_file_seat const *)(addon->file + addon->header.seats.offset);
	long seat_index;

	for (seat_index = 0; seat_index < (long)addon->header.seats.count; seat_index++)
	{
		struct addon_file_seat seat;
		long index;

		csmemcpy(&seat, &seats[seat_index], sizeof(seat));
		seat.label[ADDON_FILE_STRING_BYTES - 1] = 0;
		if (seat.element_offset > addon->header.tag_data.count ||
			addon->header.tag_data.count - seat.element_offset < UNIT_SEAT_BYTES ||
			seat.first_animation > addon->header.seat_animations.count ||
			seat.animation_count > addon->header.seat_animations.count - seat.first_animation)
		{
			error(_error_silent, "add-ons: %s's file is damaged (a seat outside it)", addon->name);
			return FALSE;
		}
		for (index = 0; index < addons_globals.original_count; index++)
		{
			struct addon_tag_instance const *biped = &addons_globals.original_instances[index];
			struct object_definition const *definition = biped->base_address;
			long graph_index;
			struct addon_tag_instance const *graph;

			if (biped->group_tag != BIPED_GROUP_TAG || !definition)
				continue;
			graph_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(definition->object.animation_graph.index);
			if (definition->object.animation_graph.index == NONE || graph_index >= addons_globals.original_count)
				continue;
			graph = &addons_globals.original_instances[graph_index];
			/* (once for each graph, which bipeds share: one given the seat
			has it) */
			if (graph->group_tag != ANIMATION_GRAPH_GROUP_TAG || graph_has_seat(graph->base_address, seat.label))
				continue;
			if (!addon_seat_add(addon, &seat, graph->base_address, graph->name))
				return FALSE;
		}
	}

	return TRUE;
}

/* (the name of the tag a handle names in the table with the add-ons) */
static char *tag_name_of(
	long tag_index)
{
	long absolute_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index);

	if (addons_globals.instances && absolute_index < addons_globals.count_with_addons)
		return addons_globals.instances[absolute_index].name;
	if (absolute_index < addons_globals.original_count)
		return addons_globals.original_instances[absolute_index].name;

	return "";
}

/* a block of elements of element_size grown by one, its elements copied to
memory of the add-ons' (*placed): the new element, or NULL */
static byte *block_grow(
	struct tag_block *block,
	struct tag_block *original,
	byte **placed,
	long element_size,
	long maximum)
{
	byte *elements;

	if (block->count >= maximum)
		return NULL;
	elements = malloc((block->count + 1) * element_size);
	if (!elements)
		return NULL;
	if (!*placed)
		*original = *block;
	csmemcpy(elements, block->address, block->count * element_size);
	csmemset(elements + block->count * element_size, 0, element_size);
	if (*placed)
		free(*placed);
	*placed = elements;
	block->address = elements;
	block->count++;

	return elements + (block->count - 1) * element_size;
}

/* the vehicles the mutators on place on this map, added to its scenario */
static void addons_place(
	char const *map_name,
	struct scenario *scenario)
{
	static struct port_mutator_place places[MAXIMUM_ADDON_PLACES];
	long place_count = port_mutators_places(places, MAXIMUM_ADDON_PLACES);
	long index;

	for (index = 0; index < place_count; index++)
	{
		struct port_mutator_place const *place = &places[index];
		long vehicle = NONE;
		short palette_index;
		byte *placement;
		struct tag_reference *reference;
		short addon_index;

		if (_stricmp(place->map, map_name))
			continue;
		for (addon_index = 0; addon_index < addons_globals.count && vehicle == NONE; addon_index++)
		{
			struct addon const *addon = &addons_globals.addons[addon_index];
			long tag;

			for (tag = 0; tag < addon->tag_count; tag++)
			{
				if (addon->tag_header->instances[tag].group_tag == VEHICLE_GROUP_TAG &&
					!_stricmp(addon->tag_header->instances[tag].name, place->vehicle))
				{
					vehicle = addon->handles[tag];
					break;
				}
			}
		}
		if (vehicle == NONE)
			vehicle = map_tag_find(VEHICLE_GROUP_TAG, place->vehicle);
		if (vehicle == NONE)
		{
			error(_error_silent, "add-ons: no vehicle '%s' to place on %s", place->vehicle, map_name);
			continue;
		}
		/* its palette entry, the scenario's or a new one */
		for (palette_index = 0; palette_index < scenario->vehicle_palette.count; palette_index++)
		{
			reference = (struct tag_reference *)((byte *)scenario->vehicle_palette.address +
				palette_index * VEHICLE_PALETTE_ENTRY_BYTES);
			if (reference->index == vehicle)
				break;
		}
		if (palette_index == scenario->vehicle_palette.count)
		{
			reference = (struct tag_reference *)block_grow(&scenario->vehicle_palette,
				&addons_globals.original_vehicle_palette, &addons_globals.placed_vehicle_palette,
				VEHICLE_PALETTE_ENTRY_BYTES, MAXIMUM_VEHICLE_PALETTE_ENTRIES);
			if (!reference)
			{
				error(_error_silent, "add-ons: %s's vehicle palette is full", map_name);
				continue;
			}
			addons_globals.vehicle_palette = &scenario->vehicle_palette;
			reference->group_tag = VEHICLE_GROUP_TAG;
			reference->name = tag_name_of(vehicle);
			reference->name_length = (long)strlen(reference->name);
			reference->index = vehicle;
		}
		/* the placement: as the scenario's first vehicle is placed (its bsps,
		its permutation), at the place's position and facing */
		placement = block_grow(&scenario->vehicles, &addons_globals.original_vehicles, &addons_globals.placed_vehicles,
			VEHICLE_PLACEMENT_BYTES, MAXIMUM_VEHICLE_PLACEMENTS);
		if (!placement)
		{
			error(_error_silent, "add-ons: %s has as many vehicles placed as it can have", map_name);
			continue;
		}
		addons_globals.vehicles = &scenario->vehicles;
		if (scenario->vehicles.count > 1)
			csmemcpy(placement, scenario->vehicles.address, VEHICLE_PLACEMENT_BYTES);
		{
			struct scenario_object_datum *object = (struct scenario_object_datum *)placement;

			object->palette_entry_index = palette_index;
			object->name_index = NONE;
			object->position.x = place->position[0];
			object->position.y = place->position[1];
			object->position.z = place->position[2];
			object->rotation.yaw = place->facing * (real)(3.14159265358979323846 / 180.0);
			object->rotation.pitch = 0.f;
			object->rotation.roll = 0.f;
		}
		error(_error_silent, "add-ons: '%s' placed on %s at (%.3f, %.3f, %.3f), facing %.1f degrees", place->vehicle,
			map_name, place->position[0], place->position[1], place->position[2], place->facing);
	}

	return;
}

static void addon_dispose(
	struct addon *addon)
{
	if (addon->geometry)
		custom_edition_addon_models_dispose(addon->geometry);
	if (addon->handles)
		free(addon->handles);
	if (addon->imports)
		free(addon->imports);
	if (addon->tags)
		free(addon->tags);
	if (addon->file)
		free(addon->file);
	csmemset(addon, 0, sizeof(*addon));

	return;
}

/* the add-on of the tag of absolute index, or NULL */
static struct addon const *addon_of(
	long tag_index)
{
	long absolute_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index);
	short index;

	if (tag_index == NONE)
		return NULL;
	for (index = 0; index < addons_globals.count; index++)
	{
		struct addon const *addon = &addons_globals.addons[index];

		if (absolute_index >= addon->first_index && absolute_index < addon->first_index + addon->tag_count &&
			addon->handles[absolute_index - addon->first_index] == tag_index)
		{
			return addon;
		}
	}

	return NULL;
}

/* ---------- public code */

void addons_tags_loaded(
	char const *map_name,
	long scenario_index)
{
	long addon_count = port_mutators_addon_count();
	long first_index;
	long first_salt = 0;
	long index;
	short addon_index;

	csmemset(&addons_globals, 0, sizeof(addons_globals));
	/* (the main menu's map plays none) */
	if (!addon_count || !_stricmp(map_name, "ui"))
		return;
	addons_globals.original_instances = cache_files_tag_instances(&addons_globals.original_count);
	if (!addons_globals.original_instances)
		return;
	for (index = 0; index < addons_globals.original_count; index++)
	{
		long salt = (long)((unsigned long)addons_globals.original_instances[index].tag_index >> 16);

		if (salt > first_salt)
			first_salt = salt;
	}
	first_salt++;
	first_index = addons_globals.original_count;

	for (index = 0; index < addon_count && addons_globals.count < MAXIMUM_ADDONS; index++)
	{
		struct addon *addon = &addons_globals.addons[addons_globals.count];

		if (addon_file_read(addon, index) &&
			first_index + addon->tag_count <= UNSIGNED_SHORT_MAX / 2 &&
			addon_tags_check(addon) &&
			addon_imports_find(addon) &&
			addon_handles_make(addon, first_index, first_salt) &&
			addon_fields_walk(addon))
		{
			addon_resources_name(addon);
			if (addon_models_convert(addon))
			{
				error(_error_silent, "add-ons: %s: %ld tags added after the map's (from %ld), %lu of the map's named",
					addon->name, addon->tag_count, first_index, (unsigned long)addon->header.imports.count);
				first_index += addon->tag_count;
				first_salt += addon->tag_count;
				addons_globals.count++;
				continue;
			}
		}
		error(_error_silent, "add-ons: %s is not added to %s", addon->name, map_name);
		addon_dispose(addon);
	}
	if (!addons_globals.count)
		return;

	/* the table with the add-ons' tags after the map's */
	addons_globals.count_with_addons = first_index;
	addons_globals.instances = malloc(first_index * sizeof(*addons_globals.instances));
	if (!addons_globals.instances)
	{
		error(_error_silent, "add-ons: out of memory for the tag table");
		for (addon_index = 0; addon_index < addons_globals.count; addon_index++)
			addon_dispose(&addons_globals.addons[addon_index]);
		addons_globals.count = 0;
		return;
	}
	csmemcpy(addons_globals.instances, addons_globals.original_instances,
		addons_globals.original_count * sizeof(*addons_globals.instances));
	for (addon_index = 0; addon_index < addons_globals.count; addon_index++)
	{
		struct addon const *addon = &addons_globals.addons[addon_index];

		for (index = 0; index < addon->tag_count; index++)
		{
			struct addon_tag_instance *instance = &addons_globals.instances[addon->first_index + index];

			*instance = addon->tag_header->instances[index];
			instance->group_tag = table_group(instance->group_tag);
			if (instance->group_tag == MODEL_GROUP_TAG)
			{
				struct tag_schema_group const *group = tag_walk_group(MODEL_GROUP_TAG, FALSE);

				instance->parent_group_tags[0] = group ? (long)group->parent_group_tags[0] : NONE;
				instance->parent_group_tags[1] = group ? (long)group->parent_group_tags[1] : NONE;
			}
			/* (a tag the map has is the map's: this one is named by none) */
			instance->tag_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(addon->handles[index]) == addon->first_index + index ?
				addon->handles[index] : DATUM_INDEX_NEW(addon->first_index + index, 0);
		}
	}
	cache_files_set_tag_instances(addons_globals.instances, addons_globals.count_with_addons);

	for (addon_index = 0; addon_index < addons_globals.count; addon_index++)
	{
		if (!addon_seats_add(&addons_globals.addons[addon_index]))
			error(_error_silent, "add-ons: %s's riders sit without their seats' animations",
				addons_globals.addons[addon_index].name);
	}
	if (scenario_index != NONE && DATUM_INDEX_TO_ABSOLUTE_INDEX(scenario_index) < addons_globals.original_count)
		addons_place(map_name, addons_globals.original_instances[DATUM_INDEX_TO_ABSOLUTE_INDEX(scenario_index)].base_address);

	return;
}

void addons_tags_unloaded(
	void)
{
	short index;

	for (index = addons_globals.seat_change_count - 1; index >= 0; index--)
	{
		struct addon_seat_change *change = &addons_globals.seat_changes[index];
		short copy;

		*change->seats = change->original;
		for (copy = 0; copy < change->copy_count; copy++)
			free(change->copies[copy]);
		free(change->elements);
	}
	if (addons_globals.placed_vehicles)
	{
		*addons_globals.vehicles = addons_globals.original_vehicles;
		free(addons_globals.placed_vehicles);
	}
	if (addons_globals.placed_vehicle_palette)
	{
		*addons_globals.vehicle_palette = addons_globals.original_vehicle_palette;
		free(addons_globals.placed_vehicle_palette);
	}
	if (addons_globals.instances)
	{
		cache_files_set_tag_instances(addons_globals.original_instances, addons_globals.original_count);
		free(addons_globals.instances);
	}
	for (index = 0; index < addons_globals.count; index++)
		addon_dispose(&addons_globals.addons[index]);
	csmemset(&addons_globals, 0, sizeof(addons_globals));

	return;
}

boolean addons_vehicle(
	long definition_index)
{
	return addon_of(definition_index) != NULL;
}

boolean addons_bitmap_contains(
	struct bitmap_data const *bitmap)
{
	return bitmap && addon_of(bitmap->tag_index) != NULL;
}

boolean addons_read(
	long tag_index,
	long offset,
	long size,
	void *buffer)
{
	struct addon const *addon = addon_of(tag_index);

	if (!addon)
		return FALSE;
	/* (the validator checked every offset the tags hold: these are checked
	again, the game reading what it is given) */
	if (offset < 0 || size < 0 || (unsigned long)offset > addon->header.resources.count ||
		(unsigned long)size > addon->header.resources.count - (unsigned long)offset)
	{
		error(_error_silent, "add-ons: a read of %ld bytes at %ld outside %s's resources", size, offset, addon->name);
		csmemset(buffer, 0, size > 0 ? size : 0);
		return TRUE;
	}
	csmemcpy(buffer, addon->file + addon->header.resources.offset + offset, size);

	return TRUE;
}
