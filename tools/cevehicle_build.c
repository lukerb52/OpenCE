/*
CEVEHICLE_BUILD.C

The add-ons' builder (docs/addons.md): a vehicle and every tag it uses,
taken from a Halo Custom Edition map, written as a .cevehicle file
(port/linux/game/addon_file.h) that mutators add to the maps they are
played on. The map is read, converted and checked as the game reads one
(port/linux/game/custom_edition_cache.c, as tools/map_validate.c does), then
the vehicle's tag and every tag its tags name are walked through their
groups' schemas (port/linux/game/tag_walk.c). A tag that every stock map
given has (by its group and name) is the map's own where the add-on plays,
an import; the rest are the add-on's, and are written with the pixels,
samples and model geometry they use. The seats the vehicle's riders sit in
are written from the riders' animation graph in the map (--riders), their
animations by name.

usage: cevehicle_build --map map.map [--maps folder] --vehicle name
	--riders graph [--stock map.map]... [--out file.cevehicle] [--report]

Built by ninja linux as build/linux/cevehicle_build, as map_validate is.
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "zlib_prefixed.h"
#include "../port/linux/game/cache_file_formats.h"
#include "../port/linux/game/addon_file.h"

/* ---------- constants */

enum
{
	/* where the game reads an Xbox map's tags (physical_memory_map.c) */
	XBOX_TAG_CACHE_ADDRESS = 0x803A6000,

	CACHE_FILE_HEADER_SIZE = 0x800,
	CACHE_FILE_HEADER_SIGNATURE = 'head',
	CACHE_FILE_FOOTER_SIGNATURE = 'foot',

	/* where a Custom Edition map's sounds decoded to Xbox ADPCM, its
	bitmaps.map and its sounds.map count in the offsets of its tags (as the
	game has them: custom_edition_cache.c) */
	CUSTOM_EDITION_DECODED_OFFSET = 0x30000000,
	CUSTOM_EDITION_BITMAPS_OFFSET = 0x40000000,
	CUSTOM_EDITION_SOUNDS_OFFSET = 0x60000000,

	VEHICLE_GROUP_TAG = 'vehi',
	BITMAP_GROUP_TAG = 'bitm',
	SOUND_GROUP_TAG = 'snd!',
	ANIMATION_GRAPH_GROUP_TAG = 'antr',
	/* a Custom Edition map's models, which an Xbox map has as models */
	GBXMODEL_GROUP_TAG = 'mod2',
	MODEL_GROUP_TAG = 'mode',

	MAXIMUM_STOCK_MAPS = 32,
	/* a tag index's index in its handle */
	TAG_HANDLE_INDEX_MASK = 0xFFFF,
	NONE = -1,

	/* a unit's seats (unit_definitions.h: _unit_definition's seats, the end
	of a unit) and a seat's label */
	VEHICLE_SEATS_OFFSET = 0x2E4,
	UNIT_SEAT_BYTES = 0x11C,
	UNIT_SEAT_LABEL_OFFSET = 4,
	/* an animation graph (model_animation_definitions.h): its unit seats
	and animations, a seat's, a weapon class's and a weapon type's blocks */
	ANIMATION_GRAPH_UNIT_SEATS_OFFSET = 0xC,
	ANIMATION_GRAPH_ANIMATIONS_OFFSET = 0x74,
	ANIMATION_BYTES = 0xB4,
	ANIMATION_GRAPH_UNIT_SEAT_BYTES = 0x64,
	UNIT_SEAT_ANIMATIONS_OFFSET = 0x40,
	UNIT_SEAT_IK_POINTS_OFFSET = 0x4C,
	UNIT_SEAT_WEAPON_CLASSES_OFFSET = 0x58,
	WEAPON_CLASS_BYTES = 0xBC,
	WEAPON_CLASS_ANIMATIONS_OFFSET = 0x98,
	WEAPON_CLASS_IK_POINTS_OFFSET = 0xA4,
	WEAPON_CLASS_WEAPON_TYPES_OFFSET = 0xB0,
	WEAPON_TYPE_BYTES = 0x3C,
	WEAPON_TYPE_ANIMATIONS_OFFSET = 0x30,
	IK_POINT_BYTES = 0x40,
	ANIMATION_INDEX_BYTES = 2,

	/* a bitmap's bitmaps (bitmap_group.h: bitmap_data) */
	BITMAP_DATA_BYTES = 0x30,
	BITMAP_DATA_PIXELS_OFFSET = 0x18,
	BITMAP_DATA_PIXELS_SIZE = 0x1C,
	BITMAP_DATA_TAG_INDEX = 0x20,
	BITMAP_DATA_HARDWARE_FORMAT = 0x28,
	BITMAP_DATA_BASE_ADDRESS = 0x2C,

	/* a sound's pitch ranges and their permutations, and where a permutation
	names its own tag (cache_file_formats.c) */
	SOUND_PITCH_RANGES_OFFSET = 0x98,
	SOUND_PITCH_RANGE_BYTES = 0x48,
	SOUND_PITCH_RANGE_PERMUTATIONS_OFFSET = 0x3C,
	SOUND_PERMUTATION_BYTES = 0x7C,
	SOUND_PERMUTATION_CACHE_TAG_INDEX_OFFSET = 0x34,
	SOUND_PERMUTATION_RUNTIME_TAG_INDEX_OFFSET = 0x3C,

	/* a Custom Edition model part (custom_edition_geometry.c:
	custom_edition_model_part) and an uncompressed model vertex */
	MODEL_PART_BYTES = 0x84,
	MODEL_PART_STRIP_TRIANGLE_COUNT = 0x48,
	MODEL_PART_STRIP_OFFSET = 0x4C,
	MODEL_PART_VERTEX_COUNT = 0x58,
	MODEL_PART_VERTEX_OFFSET = 0x64,
	MODEL_VERTEX_UNCOMPRESSED_BYTES = 0x44,

	/* a tag data's fields (tag_groups.h: tag_data) */
	TAG_DATA_SIZE = 0,
	TAG_DATA_FILE_OFFSET = 8,
	TAG_DATA_ADDRESS = 12,
	TAG_DATA_DEFINITION = 16,
	/* a tag block's and a tag reference's */
	TAG_BLOCK_ADDRESS = 4,
	TAG_BLOCK_DEFINITION = 8,
	TAG_REFERENCE_NAME = 4,
	TAG_REFERENCE_NAME_LENGTH = 8,
	TAG_REFERENCE_INDEX = 12,

	/* the sections' alignment in the file */
	SECTION_ALIGNMENT = 16,
};

/* the schema's field types (port/linux/game/tag_schema.h) */
enum
{
	_tag_schema_block,
	_tag_schema_data,
	_tag_schema_file_data,
	_tag_schema_reference,
	_tag_schema_tag_index,
};

/* ---------- structures */

struct cache_file_header
{
	unsigned long header_signature;
	long version;
	long file_length;
	unsigned long reserved0C;
	long tag_data_offset;
	long tag_data_size;
	unsigned char reserved18[8];
	char name[0x20];
	char build[0x20];
	unsigned char reserved60[0x79C];
	unsigned long footer_signature;
};

struct tag_instance
{
	unsigned long group_tag;
	unsigned long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

struct tag_header
{
	struct tag_instance *instances;
	long scenario_tag_index;
	unsigned long checksum;
	long tag_count;
};

struct tag_block
{
	long count;
	void *address;
	void *definition;
};

/* (the schema's definitions, groups and fields, as tag_schema.h lays them
out) */
struct tag_schema_definition
{
	char const *name;
	long size;
	void const *fields;
};

struct tag_schema_group
{
	unsigned long group_tag;
	unsigned long parent_group_tags[2];
	struct tag_schema_definition const *definition;
};

struct tag_schema_field
{
	short type;
	short size;
	short count;
	short flags;
	long offset;
	long maximum;
	long target_level;
	long target_offset;
	void const *definition;
	void *check;
	char const *name;
};

typedef char verify_cache_file_header_size[sizeof(struct cache_file_header) == CACHE_FILE_HEADER_SIZE ? 1 : -1];
typedef char verify_tag_instance_size[sizeof(struct tag_instance) == 0x20 ? 1 : -1];

/* a file read whole */
struct memory_file
{
	unsigned char *bytes;
	unsigned long size;
	struct cache_file_source source;
};

/* a stock map's tags: their groups and names */
struct stock_map
{
	char const *path;
	long tag_count;
	unsigned long *group_tags;
	char **names;
};

/* bytes that grow */
struct buffer
{
	unsigned char *bytes;
	unsigned long size;
	unsigned long capacity;
};

/* bytes of the tag cache a kept tag holds (its root, a block's elements,
data), and where the tag data has them */
struct range
{
	unsigned long address;
	unsigned long size;
	unsigned long offset;
};

/* a field of a kept tag that points or names a tag */
struct field
{
	short type;
	unsigned long address;
};

/* ---------- the game's (port/linux/game: tag_validate.c, tag_walk.c) */

typedef void (*tag_walk_proc)(void *context, struct tag_schema_field const *field, void *address);
unsigned char tag_walk(void *root, unsigned long group_tag, unsigned char custom_edition, tag_walk_proc proc,
	void *context);
struct tag_schema_group const *tag_walk_group(unsigned long group_tag, unsigned char custom_edition);
struct tag_validate_file_range
{
	unsigned long offset;
	unsigned long size;
};
unsigned char tag_validate_custom_edition_tags(void *tag_header, long loaded_size, unsigned long tag_cache_size,
	struct tag_validate_file_range const *file_ranges, short file_range_count, char const *map_name);
long tag_validate_corrections(void);
/* (custom_edition_sounds.c's) */
unsigned char custom_edition_sounds_decode(unsigned char *tag_cache, unsigned long loaded_bytes, long decoded_offset,
	unsigned long decoded_limit);
unsigned long custom_edition_sounds_decoded_bytes(void);
unsigned char custom_edition_sounds_read(long offset, long size, void *buffer);

static int quiet;
/* the map being read, for custom_edition_cache_read */
static struct build *reading;

void tag_validate_report(char const *message)
{
	if (!quiet)
		printf("%s\n", message);
}

/* (the game's printf family and string functions, which its units call by
these names, as tools/map_validate.c gives them) */
int halo_linux_vsnprintf(char *buffer, size_t count, char const *format, va_list arguments)
{
	return vsnprintf(buffer, count, format, arguments);
}

int halo_linux_snprintf(char *buffer, size_t count, char const *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = vsnprintf(buffer, count, format, arguments);
	va_end(arguments);

	return result;
}

void *csmemset(void *buffer, long c, unsigned long size)
{
	return memset(buffer, (int)c, size);
}

void *csmemcpy(void *destination, void const *source, unsigned long size)
{
	return memcpy(destination, source, size);
}

unsigned long csstrlen(char const *s)
{
	return strlen(s);
}

char *csstrcat(char *s1, char const *s2)
{
	return strcat(s1, s2);
}

long csstrcmp(char const *s1, char const *s2)
{
	return strcmp(s1, s2);
}

/* (the game's memory and errors, which custom_edition_sounds.c calls) */
void *debug_malloc(unsigned int size, unsigned char clear, char const *file, long line)
{
	(void)file;
	(void)line;
	return clear ? calloc(1, size) : malloc(size);
}

void *debug_realloc(void *pointer, unsigned int size, char const *file, long line)
{
	(void)file;
	(void)line;
	return realloc(pointer, size);
}

void debug_free(void *pointer, char const *file, long line)
{
	(void)file;
	(void)line;
	free(pointer);
}

void error(short priority, char const *format, ...)
{
	va_list arguments;

	(void)priority;
	if (quiet)
		return;
	va_start(arguments, format);
	vprintf(format, arguments);
	va_end(arguments);
	printf("\n");
}

/* ---------- files and buffers */

static int memory_file_read(void *context, uint32_t offset, uint32_t size, void *buffer)
{
	struct memory_file const *file = context;

	if (offset > file->size || size > file->size - offset)
		return 0;
	memcpy(buffer, file->bytes + offset, size);
	return 1;
}

static int memory_file_open(struct memory_file *file, char const *path)
{
	FILE *stream = fopen(path, "rb");
	long size;

	memset(file, 0, sizeof(*file));
	if (!stream)
		return 0;
	if (fseek(stream, 0, SEEK_END) || (size = ftell(stream)) < 0 || fseek(stream, 0, SEEK_SET) ||
		!(file->bytes = malloc((size_t)size + 1)) || fread(file->bytes, 1, (size_t)size, stream) != (size_t)size)
	{
		free(file->bytes);
		file->bytes = NULL;
		fclose(stream);
		return 0;
	}
	fclose(stream);
	file->size = (unsigned long)size;
	file->source.context = file;
	file->source.read = memory_file_read;
	file->source.size = (uint32_t)size;
	return 1;
}

static void out_of_memory(void)
{
	fprintf(stderr, "cevehicle_build: out of memory\n");
	exit(2);
}

/* room for size more bytes at the buffer's end, zeroed: their offset */
static unsigned long buffer_grow(struct buffer *buffer, unsigned long size)
{
	unsigned long offset = buffer->size;

	if (buffer->size + size > buffer->capacity)
	{
		unsigned long capacity = buffer->capacity ? buffer->capacity : 0x10000;

		while (capacity < buffer->size + size)
			capacity *= 2;
		if (!(buffer->bytes = realloc(buffer->bytes, capacity)))
			out_of_memory();
		buffer->capacity = capacity;
	}
	memset(buffer->bytes + offset, 0, size);
	buffer->size += size;
	return offset;
}

static unsigned long buffer_add(struct buffer *buffer, void const *bytes, unsigned long size)
{
	unsigned long offset = buffer_grow(buffer, size);

	memcpy(buffer->bytes + offset, bytes, size);
	return offset;
}

static void buffer_align(struct buffer *buffer, unsigned long alignment)
{
	if (buffer->size % alignment)
		buffer_grow(buffer, alignment - buffer->size % alignment);
}

static uint32_t read_u32(unsigned char const *bytes)
{
	uint32_t value;

	memcpy(&value, bytes, sizeof(value));
	return value;
}

static void write_u32(unsigned char *bytes, uint32_t value)
{
	memcpy(bytes, &value, sizeof(value));
}

/* ---------- the stock maps */

static char const *tag_text(unsigned long group_tag)
{
	static char text[5];

	text[0] = (char)(group_tag >> 24);
	text[1] = (char)(group_tag >> 16);
	text[2] = (char)(group_tag >> 8);
	text[3] = (char)group_tag;
	text[4] = 0;
	return text;
}

/* an Xbox map's tag groups and names, read from its file (inflated after its
header if it is compressed): 0 if they cannot be */
static int stock_map_read(struct stock_map *map, char const *path)
{
	FILE *file = fopen(path, "rb");
	struct cache_file_header header;
	unsigned char *bytes = NULL;
	unsigned char *compressed = NULL;
	long size;
	int read = 0;

	memset(map, 0, sizeof(*map));
	map->path = path;
	if (!file)
		return 0;
	if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < CACHE_FILE_HEADER_SIZE || fseek(file, 0, SEEK_SET) ||
		fread(&header, CACHE_FILE_HEADER_SIZE, 1, file) != 1 ||
		header.header_signature != CACHE_FILE_HEADER_SIGNATURE ||
		header.footer_signature != CACHE_FILE_FOOTER_SIGNATURE ||
		header.version != CACHE_FILE_VERSION_XBOX || header.file_length < CACHE_FILE_HEADER_SIZE)
	{
		fclose(file);
		return 0;
	}
	bytes = malloc(header.file_length);
	compressed = malloc(size - CACHE_FILE_HEADER_SIZE);
	if (bytes && compressed && fread(compressed, size - CACHE_FILE_HEADER_SIZE, 1, file) == 1)
	{
		uLongf inflated = header.file_length - CACHE_FILE_HEADER_SIZE;

		memcpy(bytes, &header, CACHE_FILE_HEADER_SIZE);
		if (size == header.file_length)
			memcpy(bytes + CACHE_FILE_HEADER_SIZE, compressed, size - CACHE_FILE_HEADER_SIZE);
		else if (uncompress(bytes + CACHE_FILE_HEADER_SIZE, &inflated, compressed, size - CACHE_FILE_HEADER_SIZE) !=
			Z_OK || inflated != (uLongf)(header.file_length - CACHE_FILE_HEADER_SIZE))
		{
			goto done;
		}
	}
	else
	{
		goto done;
	}
	/* (its tag header and instances, at their addresses in the tag cache:
	every one checked to lie in the tag data) */
	if (header.tag_data_offset >= CACHE_FILE_HEADER_SIZE && header.tag_data_size >= (long)sizeof(struct tag_header) &&
		header.tag_data_offset <= header.file_length - header.tag_data_size)
	{
		unsigned char *tags = bytes + header.tag_data_offset;
		struct tag_header const *tag_header = (struct tag_header const *)tags;
		unsigned long instances = (unsigned long)tag_header->instances - XBOX_TAG_CACHE_ADDRESS;
		long index;

		if (tag_header->tag_count <= 0 || tag_header->tag_count > 0xFFFF ||
			instances > (unsigned long)header.tag_data_size ||
			(unsigned long)tag_header->tag_count * sizeof(struct tag_instance) > header.tag_data_size - instances)
		{
			goto done;
		}
		map->tag_count = tag_header->tag_count;
		map->group_tags = calloc((size_t)map->tag_count, sizeof(*map->group_tags));
		map->names = calloc((size_t)map->tag_count, sizeof(*map->names));
		if (!map->group_tags || !map->names)
			goto done;
		for (index = 0; index < map->tag_count; index++)
		{
			struct tag_instance const *instance = (struct tag_instance const *)(tags + instances) + index;
			unsigned long name = (unsigned long)instance->name - XBOX_TAG_CACHE_ADDRESS;

			if (name >= (unsigned long)header.tag_data_size || !memchr(tags + name, 0, header.tag_data_size - name))
				goto done;
			map->group_tags[index] = instance->group_tag;
			map->names[index] = strdup((char const *)tags + name);
		}
		read = 1;
	}

done:
	free(compressed);
	free(bytes);
	fclose(file);
	return read;
}

static int stock_map_has(struct stock_map const *map, unsigned long group_tag, char const *name)
{
	long index;

	if (group_tag == GBXMODEL_GROUP_TAG)
		group_tag = MODEL_GROUP_TAG;
	for (index = 0; index < map->tag_count; index++)
	{
		if (map->group_tags[index] == group_tag && map->names[index] && !strcasecmp(map->names[index], name))
			return 1;
	}
	return 0;
}

/* ---------- the vehicle's tags */

enum
{
	_tag_unvisited = 0,
	/* the add-on's own */
	_tag_kept,
	/* every stock map's: the map's own where the add-on plays */
	_tag_imported,
};

struct build
{
	unsigned char *tag_cache;
	unsigned long loaded_bytes;
	struct tag_header *header;
	/* the map and its resource maps, and where its model data is */
	struct memory_file map;
	struct memory_file resource_files[NUMBER_OF_RESOURCE_MAP_TYPES];
	struct custom_edition_load_report report;
	struct stock_map stock[MAXIMUM_STOCK_MAPS];
	short stock_count;

	/* each tag's state, the kept tags in the order they were found (the
	add-on's tags' order), and the imports in theirs */
	unsigned char *states;
	long *kept;
	long kept_count;
	long walked;
	long *imports;
	long import_count;
	/* each tag's index among the kept or the imports */
	long *indices;

	/* what the kept tags hold, as the walk found it (the tag being walked,
	for the fields only some groups have) */
	unsigned long walking_group;
	struct range *ranges;
	long range_count;
	long range_capacity;
	struct field *fields;
	long field_count;
	long field_capacity;
	unsigned long *bitmap_blocks;
	long bitmap_block_count;
	unsigned long *part_blocks;
	long part_block_count;
	/* a tag of a group no schema has (the walk cannot go on) */
	long unwalkable;

	/* the file's sections */
	struct buffer tag_data;
	struct buffer relocations;
	struct buffer import_fields;
	struct buffer resources;
	struct buffer model_vertices;
	struct buffer model_strips;
	struct buffer seats;
	struct buffer seat_animations;
	/* bytes of resources already written, by where they came from */
	struct range *resource_ranges;
	long resource_range_count;
	long resource_range_capacity;
	/* the tag data's empty name, and each tag's name in it */
	unsigned long *name_offsets;
	/* the kept sounds the conversion left without pitch ranges (Ogg Vorbis,
	which this build cannot decode) */
	long silent_sounds;
	long problems;
};

static char const *tag_name_of(struct build const *build, long index)
{
	return build->header->instances[index].name;
}

static void problem(struct build *build, char const *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	vprintf(format, arguments);
	va_end(arguments);
	printf("\n");
	build->problems++;
}

static void *array_grow(void *array, long *capacity, long count, size_t element_size)
{
	if (count < *capacity)
		return array;
	*capacity = *capacity ? *capacity * 2 : 256;
	if (!(array = realloc(array, (size_t)*capacity * element_size)))
		out_of_memory();
	return array;
}

/* the tag a handle names, its state decided if it is new */
static void tag_visit(struct build *build, long handle)
{
	long index = handle & TAG_HANDLE_INDEX_MASK;
	struct tag_instance const *instance;
	short map;

	if (handle == NONE || index >= build->header->tag_count || build->states[index] != _tag_unvisited)
		return;
	instance = &build->header->instances[index];
	build->states[index] = build->stock_count ? _tag_imported : _tag_kept;
	for (map = 0; map < build->stock_count; map++)
	{
		if (!stock_map_has(&build->stock[map], instance->group_tag, instance->name))
		{
			build->states[index] = _tag_kept;
			break;
		}
	}
	/* (an import's own tags are the map's too: only the kept are walked) */
	if (build->states[index] == _tag_kept)
	{
		build->indices[index] = build->kept_count;
		build->kept[build->kept_count++] = index;
	}
	else
	{
		build->indices[index] = build->import_count;
		build->imports[build->import_count++] = index;
	}
}

static void range_add(struct build *build, void const *address, unsigned long size)
{
	struct range *range;

	if (!size)
		return;
	build->ranges = array_grow(build->ranges, &build->range_capacity, build->range_count, sizeof(*build->ranges));
	range = &build->ranges[build->range_count++];
	range->address = (unsigned long)address;
	range->size = size;
	range->offset = 0;
}

static void field_add(struct build *build, short type, void const *address)
{
	struct field *field;

	build->fields = array_grow(build->fields, &build->field_capacity, build->field_count, sizeof(*build->fields));
	field = &build->fields[build->field_count++];
	field->type = type;
	field->address = (unsigned long)address;
}

static void field_visit(void *context, struct tag_schema_field const *field, void *address)
{
	struct build *build = context;

	switch (field->type)
	{
	case _tag_schema_block:
	{
		struct tag_block const *block = address;
		struct tag_schema_definition const *element = field->definition;

		field_add(build, field->type, address);
		if (block->count > 0)
			range_add(build, block->address, (unsigned long)block->count * (unsigned long)element->size);
		/* (a bitmap's bitmaps and a model's parts: their pixels and
		geometry are the add-on's too) */
		if (build->walking_group == BITMAP_GROUP_TAG && !strcmp(field->name, "bitmaps"))
		{
			build->bitmap_blocks = realloc(build->bitmap_blocks, sizeof(long) * (size_t)(build->bitmap_block_count + 1));
			if (!build->bitmap_blocks)
				out_of_memory();
			build->bitmap_blocks[build->bitmap_block_count++] = (unsigned long)address;
		}
		if (build->walking_group == GBXMODEL_GROUP_TAG && !strcmp(field->name, "parts"))
		{
			build->part_blocks = realloc(build->part_blocks, sizeof(long) * (size_t)(build->part_block_count + 1));
			if (!build->part_blocks)
				out_of_memory();
			build->part_blocks[build->part_block_count++] = (unsigned long)address;
		}
		break;
	}

	case _tag_schema_data:
		field_add(build, field->type, address);
		if (read_u32(address) > 0 && read_u32((unsigned char *)address + TAG_DATA_ADDRESS))
		{
			range_add(build, (void const *)(unsigned long)read_u32((unsigned char *)address + TAG_DATA_ADDRESS),
				read_u32(address));
		}
		break;

	case _tag_schema_file_data:
		field_add(build, field->type, address);
		break;

	case _tag_schema_reference:
		field_add(build, field->type, address);
		tag_visit(build, (long)read_u32((unsigned char *)address + TAG_REFERENCE_INDEX));
		break;

	case _tag_schema_tag_index:
		field_add(build, field->type, address);
		tag_visit(build, (long)read_u32(address));
		break;
	}
}

/* the vehicle named and every tag it uses: 0 if they cannot be walked */
static int vehicle_walk(struct build *build, char const *vehicle, long *root)
{
	long count = build->header->tag_count;
	long index;

	*root = NONE;
	build->states = calloc((size_t)count, 1);
	build->kept = calloc((size_t)count, sizeof(*build->kept));
	build->imports = calloc((size_t)count, sizeof(*build->imports));
	build->indices = calloc((size_t)count, sizeof(*build->indices));
	if (!build->states || !build->kept || !build->imports || !build->indices)
		out_of_memory();
	for (index = 0; index < count; index++)
	{
		if (build->header->instances[index].group_tag == VEHICLE_GROUP_TAG &&
			!strcasecmp(tag_name_of(build, index), vehicle))
		{
			*root = index;
		}
	}
	if (*root == NONE)
	{
		printf("the map has no vehicle '%s'\n", vehicle);
		return 0;
	}
	/* (the vehicle is the add-on's, whatever the stock maps have) */
	build->states[*root] = _tag_kept;
	build->indices[*root] = 0;
	build->kept[build->kept_count++] = *root;
	while (build->walked < build->kept_count)
	{
		struct tag_instance const *instance = &build->header->instances[build->kept[build->walked++]];
		struct tag_schema_group const *group = tag_walk_group(instance->group_tag, 1);

		build->walking_group = instance->group_tag;
		if (!group || !tag_walk(instance->base_address, instance->group_tag, 1, field_visit, build))
		{
			printf("no schema walks %s %s\n", tag_text(instance->group_tag), instance->name);
			build->unwalkable++;
			continue;
		}
		range_add(build, instance->base_address, (unsigned long)group->definition->size);
	}
	return !build->unwalkable;
}

/* ---------- the tag data */

static int range_compare(void const *a, void const *b)
{
	struct range const *first = a;
	struct range const *second = b;

	return first->address < second->address ? -1 : first->address > second->address;
}

/* the range that holds address (NULL if none does) */
static struct range const *range_of(struct build const *build, unsigned long address)
{
	long low = 0;
	long high = build->range_count - 1;

	while (low <= high)
	{
		long middle = (low + high) / 2;
		struct range const *range = &build->ranges[middle];

		if (address < range->address)
			high = middle - 1;
		else if (address >= range->address + range->size)
			low = middle + 1;
		else
			return range;
	}
	return NULL;
}

/* where the tag data has the byte at address, which a kept tag holds */
static unsigned long tag_data_offset(struct build *build, unsigned long address)
{
	struct range const *range = range_of(build, address);

	if (!range)
	{
		problem(build, "an address (%08lx) no kept tag holds", address);
		return 0;
	}
	return range->offset + (address - range->address);
}

static void relocation_add(struct build *build, unsigned long offset)
{
	uint32_t value = (uint32_t)offset;

	buffer_add(&build->relocations, &value, sizeof(value));
}

/* the bytes at offset in the map's combined offset space (the map, then
bitmaps.map, then sounds.map), or NULL unless all size of them are there */
static unsigned char const *combined_bytes(struct build *build, unsigned long offset, unsigned long size)
{
	static unsigned char *decoded;
	struct memory_file const *file = &build->map;

	/* (the sounds decoded to Xbox ADPCM, as custom_edition_sounds_read
	serves them) */
	if (offset >= CUSTOM_EDITION_DECODED_OFFSET && offset < CUSTOM_EDITION_BITMAPS_OFFSET)
	{
		free(decoded);
		decoded = malloc(size + 1);
		if (!decoded ||
			!custom_edition_sounds_read((long)(offset - CUSTOM_EDITION_DECODED_OFFSET), (long)size, decoded))
		{
			return NULL;
		}
		return decoded;
	}
	if (offset >= CUSTOM_EDITION_SOUNDS_OFFSET)
	{
		file = &build->resource_files[_resource_map_sounds];
		offset -= CUSTOM_EDITION_SOUNDS_OFFSET;
	}
	else if (offset >= CUSTOM_EDITION_BITMAPS_OFFSET)
	{
		file = &build->resource_files[_resource_map_bitmaps];
		offset -= CUSTOM_EDITION_BITMAPS_OFFSET;
	}
	if (!file->bytes || offset > file->size || size > file->size - offset)
		return NULL;
	return file->bytes + offset;
}

/* (the game's read of a Custom Edition map, which custom_edition_sounds.c
calls for a sound's samples: from the map's files) */
void custom_edition_cache_read(long tag_index, long offset, long size, void *buffer)
{
	unsigned char const *bytes = combined_bytes(reading, (unsigned long)offset, (unsigned long)size);

	(void)tag_index;
	if (bytes)
		memcpy(buffer, bytes, (size_t)size);
	else
		memset(buffer, 0, (size_t)size);
}

/* the resources' offset of size bytes at offset in the combined offset
space, written once however many name them */
static uint32_t resource_add(struct build *build, unsigned long offset, unsigned long size)
{
	unsigned char const *bytes = combined_bytes(build, offset, size);
	struct range *range;
	long index;

	if (!bytes)
	{
		problem(build, "%lu bytes at %08lx are in none of the map's files", size, offset);
		return 0;
	}
	for (index = 0; index < build->resource_range_count; index++)
	{
		if (build->resource_ranges[index].address == offset && build->resource_ranges[index].size == size)
			return (uint32_t)build->resource_ranges[index].offset;
	}
	build->resource_ranges = array_grow(build->resource_ranges, &build->resource_range_capacity,
		build->resource_range_count, sizeof(*build->resource_ranges));
	range = &build->resource_ranges[build->resource_range_count++];
	range->address = offset;
	range->size = size;
	buffer_align(&build->resources, SECTION_ALIGNMENT);
	range->offset = buffer_add(&build->resources, bytes, size);
	return (uint32_t)range->offset;
}

/* a field naming a tag, written as the add-on's (its index among the
kept), as an import field (none, the map's), or none */
static void tag_field_write(struct build *build, unsigned char *field, long handle, int reference)
{
	long index = handle & TAG_HANDLE_INDEX_MASK;
	unsigned long offset = (unsigned long)(field - build->tag_data.bytes);
	unsigned char *index_field = reference ? field + TAG_REFERENCE_INDEX : field;

	if (handle == NONE || index >= build->header->tag_count || build->states[index] == _tag_unvisited)
	{
		write_u32(index_field, (uint32_t)NONE);
		if (reference)
		{
			write_u32(field + TAG_REFERENCE_NAME, 0);
			write_u32(field + TAG_REFERENCE_NAME_LENGTH, 0);
			relocation_add(build, offset + TAG_REFERENCE_NAME);
		}
		return;
	}
	if (reference)
	{
		write_u32(field + TAG_REFERENCE_NAME, (uint32_t)build->name_offsets[index]);
		write_u32(field + TAG_REFERENCE_NAME_LENGTH, (uint32_t)strlen(tag_name_of(build, index)));
		relocation_add(build, offset + TAG_REFERENCE_NAME);
	}
	if (build->states[index] == _tag_kept)
	{
		write_u32(index_field, (uint32_t)build->indices[index]);
	}
	else
	{
		struct addon_file_import_field import;

		write_u32(index_field, (uint32_t)NONE);
		import.offset = (uint32_t)offset;
		import.import = (uint32_t)build->indices[index] | (reference ? ADDON_FILE_IMPORT_REFERENCE_BIT : 0);
		buffer_add(&build->import_fields, &import, sizeof(import));
	}
}

/* the kept tags' bytes copied into the tag data, after the names, and every
field that points or names a tag made the add-on's */
static void tag_data_write(struct build *build)
{
	long count = build->header->tag_count;
	long index;

	build->name_offsets = calloc((size_t)count, sizeof(*build->name_offsets));
	if (!build->name_offsets)
		out_of_memory();
	/* (an empty name first, for references to no tag) */
	buffer_grow(&build->tag_data, 1);
	for (index = 0; index < count; index++)
	{
		if (build->states[index] != _tag_unvisited)
		{
			char const *name = tag_name_of(build, index);

			build->name_offsets[index] = buffer_add(&build->tag_data, name, strlen(name) + 1);
		}
	}
	qsort(build->ranges, (size_t)build->range_count, sizeof(*build->ranges), range_compare);
	for (index = 0; index < build->range_count; index++)
	{
		struct range *range = &build->ranges[index];

		if (index && range->address < build->ranges[index - 1].address + build->ranges[index - 1].size)
		{
			problem(build, "the bytes at %08lx are held twice", range->address);
			continue;
		}
		buffer_align(&build->tag_data, 4);
		range->offset = buffer_add(&build->tag_data, (void const *)range->address, range->size);
	}

	for (index = 0; index < build->field_count; index++)
	{
		struct field const *source = &build->fields[index];
		unsigned long offset = tag_data_offset(build, source->address);
		unsigned char *field = build->tag_data.bytes + offset;

		switch (source->type)
		{
		case _tag_schema_block:
		{
			struct tag_block const *block = (struct tag_block const *)source->address;

			write_u32(field + TAG_BLOCK_DEFINITION, 0);
			if (block->count > 0)
			{
				write_u32(field + TAG_BLOCK_ADDRESS, (uint32_t)tag_data_offset(build, (unsigned long)block->address));
				relocation_add(build, offset + TAG_BLOCK_ADDRESS);
			}
			else
			{
				write_u32(field + TAG_BLOCK_ADDRESS, 0);
			}
			break;
		}

		case _tag_schema_data:
		{
			unsigned char const *data = (unsigned char const *)source->address;

			write_u32(field + TAG_DATA_DEFINITION, 0);
			if (read_u32(data + TAG_DATA_SIZE) > 0 && read_u32(data + TAG_DATA_ADDRESS))
			{
				write_u32(field + TAG_DATA_ADDRESS, (uint32_t)tag_data_offset(build, read_u32(data + TAG_DATA_ADDRESS)));
				relocation_add(build, offset + TAG_DATA_ADDRESS);
			}
			else
			{
				write_u32(field + TAG_DATA_ADDRESS, 0);
			}
			break;
		}

		case _tag_schema_file_data:
		{
			unsigned char const *data = (unsigned char const *)source->address;
			uint32_t size = read_u32(data + TAG_DATA_SIZE);

			write_u32(field + TAG_DATA_DEFINITION, 0);
			write_u32(field + TAG_DATA_ADDRESS, 0);
			if (size)
				write_u32(field + TAG_DATA_FILE_OFFSET, resource_add(build, read_u32(data + TAG_DATA_FILE_OFFSET), size));
			break;
		}

		case _tag_schema_reference:
			tag_field_write(build, field, (long)read_u32((unsigned char const *)source->address + TAG_REFERENCE_INDEX), 1);
			break;

		case _tag_schema_tag_index:
			tag_field_write(build, field, (long)read_u32((unsigned char const *)source->address), 0);
			break;
		}
	}
}

/* each kept bitmap's pixels written into the resources */
static void bitmaps_write(struct build *build)
{
	long block_index;

	for (block_index = 0; block_index < build->bitmap_block_count; block_index++)
	{
		struct tag_block const *block = (struct tag_block const *)build->bitmap_blocks[block_index];
		long element;

		for (element = 0; element < block->count; element++)
		{
			unsigned char const *source = (unsigned char const *)block->address + element * BITMAP_DATA_BYTES;
			unsigned char *bitmap = build->tag_data.bytes + tag_data_offset(build, (unsigned long)source);
			uint32_t size = read_u32(source + BITMAP_DATA_PIXELS_SIZE);

			write_u32(bitmap + BITMAP_DATA_PIXELS_OFFSET,
				size ? resource_add(build, read_u32(source + BITMAP_DATA_PIXELS_OFFSET), size) : 0);
			/* (the game's, as it draws: the loader gives each its tag) */
			write_u32(bitmap + BITMAP_DATA_TAG_INDEX, (uint32_t)NONE);
			write_u32(bitmap + BITMAP_DATA_HARDWARE_FORMAT, 0);
			write_u32(bitmap + BITMAP_DATA_BASE_ADDRESS, 0);
		}
	}
}

/* each kept sound's permutations naming it by its index among the kept, as
the game's check of them asks (the loader gives them its index in the map's
table) */
static void sounds_write(struct build *build)
{
	long index;

	for (index = 0; index < build->kept_count; index++)
	{
		struct tag_instance const *instance = &build->header->instances[build->kept[index]];
		struct tag_block const *pitch_ranges;
		long pitch_range;

		if (instance->group_tag != SOUND_GROUP_TAG)
			continue;
		pitch_ranges = (struct tag_block const *)((unsigned char const *)instance->base_address +
			SOUND_PITCH_RANGES_OFFSET);
		if (!pitch_ranges->count)
		{
			printf("the sound %s cannot be played (Ogg Vorbis): it is silent\n", instance->name);
			build->silent_sounds++;
		}
		for (pitch_range = 0; pitch_range < pitch_ranges->count; pitch_range++)
		{
			struct tag_block const *permutations = (struct tag_block const *)((unsigned char const *)pitch_ranges->address +
				pitch_range * SOUND_PITCH_RANGE_BYTES + SOUND_PITCH_RANGE_PERMUTATIONS_OFFSET);
			long permutation;

			for (permutation = 0; permutation < permutations->count; permutation++)
			{
				unsigned char const *source = (unsigned char const *)permutations->address +
					permutation * SOUND_PERMUTATION_BYTES;
				unsigned char *bytes = build->tag_data.bytes + tag_data_offset(build, (unsigned long)source);

				write_u32(bytes + SOUND_PERMUTATION_CACHE_TAG_INDEX_OFFSET, (uint32_t)index);
				write_u32(bytes + SOUND_PERMUTATION_RUNTIME_TAG_INDEX_OFFSET, (uint32_t)index);
			}
		}
	}
}

/* each kept model's parts' vertices and strips written into the model data */
static void models_write(struct build *build)
{
	long block_index;

	for (block_index = 0; block_index < build->part_block_count; block_index++)
	{
		struct tag_block const *block = (struct tag_block const *)build->part_blocks[block_index];
		long element;

		for (element = 0; element < block->count; element++)
		{
			unsigned char const *source = (unsigned char const *)block->address + element * MODEL_PART_BYTES;
			unsigned char *part = build->tag_data.bytes + tag_data_offset(build, (unsigned long)source);
			uint32_t vertex_count = read_u32(source + MODEL_PART_VERTEX_COUNT);
			uint32_t vertex_offset = read_u32(source + MODEL_PART_VERTEX_OFFSET);
			uint32_t strip_count = read_u32(source + MODEL_PART_STRIP_TRIANGLE_COUNT) + 2;
			uint32_t strip_offset = read_u32(source + MODEL_PART_STRIP_OFFSET);
			unsigned long model_data_offset = build->report.model_data_offset;
			unsigned long model_data_bytes = build->report.model_data_bytes;
			unsigned long index_data_offset = build->report.model_index_data_offset;

			/* (the loader checked they lie in the model data) */
			if (vertex_offset > model_data_bytes ||
				(unsigned long)vertex_count * MODEL_VERTEX_UNCOMPRESSED_BYTES > model_data_bytes - vertex_offset ||
				index_data_offset + strip_offset > model_data_bytes ||
				(unsigned long)strip_count * 2 > model_data_bytes - index_data_offset - strip_offset ||
				model_data_offset + model_data_bytes > build->map.size)
			{
				problem(build, "a model part's geometry is outside the model data");
				continue;
			}
			write_u32(part + MODEL_PART_VERTEX_OFFSET, (uint32_t)buffer_add(&build->model_vertices,
				build->map.bytes + model_data_offset + vertex_offset,
				(unsigned long)vertex_count * MODEL_VERTEX_UNCOMPRESSED_BYTES));
			write_u32(part + MODEL_PART_STRIP_OFFSET, (uint32_t)buffer_add(&build->model_strips,
				build->map.bytes + model_data_offset + index_data_offset + strip_offset, (unsigned long)strip_count * 2));
		}
	}
}

/* ---------- the riders' seats */

static unsigned char *graph_find(struct build *build, char const *name)
{
	long index;

	for (index = 0; index < build->header->tag_count; index++)
	{
		if (build->header->instances[index].group_tag == ANIMATION_GRAPH_GROUP_TAG &&
			!strcasecmp(tag_name_of(build, index), name))
		{
			return build->header->instances[index].base_address;
		}
	}
	return NULL;
}

/* a block of the riders' graph (elements of element_bytes) copied into the
tag data, the field at field_offset there made to point at the copy: its
offset */
static unsigned long seat_block_copy(struct build *build, unsigned long field_offset, struct tag_block const *block,
	unsigned long element_bytes)
{
	unsigned long offset;

	write_u32(build->tag_data.bytes + field_offset + TAG_BLOCK_DEFINITION, 0);
	if (block->count <= 0)
	{
		write_u32(build->tag_data.bytes + field_offset + TAG_BLOCK_ADDRESS, 0);
		return 0;
	}
	buffer_align(&build->tag_data, 4);
	offset = buffer_add(&build->tag_data, block->address, (unsigned long)block->count * element_bytes);
	write_u32(build->tag_data.bytes + field_offset + TAG_BLOCK_ADDRESS, (uint32_t)offset);
	relocation_add(build, field_offset + TAG_BLOCK_ADDRESS);
	return offset;
}

/* the animation indices of a block copied at offset, written as the
animations they name */
static void seat_animations_write(struct build *build, unsigned long offset, long count, unsigned char const *graph,
	struct addon_file_seat *seat)
{
	struct tag_block const *animations = (struct tag_block const *)(graph + ANIMATION_GRAPH_ANIMATIONS_OFFSET);
	long index;

	for (index = 0; index < count; index++)
	{
		unsigned char *field = build->tag_data.bytes + offset + index * ANIMATION_INDEX_BYTES;
		short animation;
		struct addon_file_seat_animation named;

		memcpy(&animation, field, sizeof(animation));
		if (animation == NONE)
			continue;
		if (animation < 0 || animation >= animations->count)
		{
			problem(build, "the seat '%s' names an animation its graph has not", seat->label);
			continue;
		}
		memset(&named, 0, sizeof(named));
		named.offset = (uint32_t)(offset + index * ANIMATION_INDEX_BYTES);
		strncpy(named.name, (char const *)animations->address + animation * ANIMATION_BYTES, ADDON_FILE_STRING_BYTES - 1);
		buffer_add(&build->seat_animations, &named, sizeof(named));
		seat->animation_count++;
		/* (the loader writes each one's index in the riders' graph) */
		animation = NONE;
		memcpy(field, &animation, sizeof(animation));
	}
}

/* every seat label of the vehicle's seats, from the riders' graph */
static void seats_write(struct build *build, long root, char const *riders)
{
	unsigned char *graph = graph_find(build, riders);
	unsigned char const *vehicle = build->header->instances[root].base_address;
	struct tag_block const *vehicle_seats = (struct tag_block const *)(vehicle + VEHICLE_SEATS_OFFSET);
	struct tag_block const *graph_seats;
	long seat_index;

	if (!graph)
	{
		problem(build, "the map has no animation graph '%s'", riders);
		return;
	}
	graph_seats = (struct tag_block const *)(graph + ANIMATION_GRAPH_UNIT_SEATS_OFFSET);
	for (seat_index = 0; seat_index < vehicle_seats->count; seat_index++)
	{
		char const *label = (char const *)vehicle_seats->address + seat_index * UNIT_SEAT_BYTES + UNIT_SEAT_LABEL_OFFSET;
		struct addon_file_seat seat;
		unsigned char const *source = NULL;
		unsigned long element;
		long index;

		/* (once for each label) */
		for (index = 0; index < (long)(build->seats.size / sizeof(seat)); index++)
		{
			if (!strncasecmp(((struct addon_file_seat const *)build->seats.bytes)[index].label, label,
				ADDON_FILE_STRING_BYTES))
			{
				break;
			}
		}
		if (index < (long)(build->seats.size / sizeof(seat)))
			continue;
		for (index = 0; index < graph_seats->count; index++)
		{
			unsigned char const *candidate = (unsigned char const *)graph_seats->address +
				index * ANIMATION_GRAPH_UNIT_SEAT_BYTES;

			if (!strncasecmp((char const *)candidate, label, ADDON_FILE_STRING_BYTES))
				source = candidate;
		}
		if (!source)
		{
			problem(build, "the riders' graph has no seat '%s'", label);
			continue;
		}
		memset(&seat, 0, sizeof(seat));
		strncpy(seat.label, label, ADDON_FILE_STRING_BYTES - 1);
		seat.first_animation = (uint32_t)(build->seat_animations.size / sizeof(struct addon_file_seat_animation));
		buffer_align(&build->tag_data, 4);
		element = buffer_add(&build->tag_data, source, ANIMATION_GRAPH_UNIT_SEAT_BYTES);
		seat.element_offset = (uint32_t)element;
		{
			struct tag_block const *animations = (struct tag_block const *)(source + UNIT_SEAT_ANIMATIONS_OFFSET);
			struct tag_block const *classes = (struct tag_block const *)(source + UNIT_SEAT_WEAPON_CLASSES_OFFSET);
			unsigned long animations_offset;
			unsigned long classes_offset;
			long class_index;

			animations_offset = seat_block_copy(build, element + UNIT_SEAT_ANIMATIONS_OFFSET, animations,
				ANIMATION_INDEX_BYTES);
			seat_animations_write(build, animations_offset, animations->count, graph, &seat);
			seat_block_copy(build, element + UNIT_SEAT_IK_POINTS_OFFSET,
				(struct tag_block const *)(source + UNIT_SEAT_IK_POINTS_OFFSET), IK_POINT_BYTES);
			classes_offset = seat_block_copy(build, element + UNIT_SEAT_WEAPON_CLASSES_OFFSET, classes,
				WEAPON_CLASS_BYTES);
			for (class_index = 0; class_index < classes->count; class_index++)
			{
				unsigned char const *class_source = (unsigned char const *)classes->address + class_index * WEAPON_CLASS_BYTES;
				unsigned long class_offset = classes_offset + class_index * WEAPON_CLASS_BYTES;
				struct tag_block const *class_animations =
					(struct tag_block const *)(class_source + WEAPON_CLASS_ANIMATIONS_OFFSET);
				struct tag_block const *types = (struct tag_block const *)(class_source + WEAPON_CLASS_WEAPON_TYPES_OFFSET);
				unsigned long types_offset;
				long type_index;

				seat_animations_write(build,
					seat_block_copy(build, class_offset + WEAPON_CLASS_ANIMATIONS_OFFSET, class_animations,
						ANIMATION_INDEX_BYTES),
					class_animations->count, graph, &seat);
				seat_block_copy(build, class_offset + WEAPON_CLASS_IK_POINTS_OFFSET,
					(struct tag_block const *)(class_source + WEAPON_CLASS_IK_POINTS_OFFSET), IK_POINT_BYTES);
				types_offset = seat_block_copy(build, class_offset + WEAPON_CLASS_WEAPON_TYPES_OFFSET, types,
					WEAPON_TYPE_BYTES);
				for (type_index = 0; type_index < types->count; type_index++)
				{
					unsigned char const *type_source = (unsigned char const *)types->address + type_index * WEAPON_TYPE_BYTES;
					struct tag_block const *type_animations =
						(struct tag_block const *)(type_source + WEAPON_TYPE_ANIMATIONS_OFFSET);
					unsigned long type_offset = types_offset + type_index * WEAPON_TYPE_BYTES;

					seat_animations_write(build,
						seat_block_copy(build, type_offset + WEAPON_TYPE_ANIMATIONS_OFFSET, type_animations,
							ANIMATION_INDEX_BYTES),
						type_animations->count, graph, &seat);
				}
			}
		}
		buffer_add(&build->seats, &seat, sizeof(seat));
	}
}

/* ---------- the file */

static uint32_t section_write(struct buffer *file, struct addon_file_section *section, void const *bytes,
	unsigned long size, unsigned long element_size)
{
	buffer_align(file, SECTION_ALIGNMENT);
	section->offset = (uint32_t)file->size;
	section->count = (uint32_t)(size / element_size);
	if (size)
		buffer_add(file, bytes, size);
	return section->offset;
}

static int file_write(struct build *build, long root, char const *path, unsigned long *file_size)
{
	struct buffer file = { NULL, 0, 0 };
	struct addon_file_header header;
	struct buffer tags = { NULL, 0, 0 };
	struct buffer imports = { NULL, 0, 0 };
	long index;
	FILE *stream;
	int written;

	for (index = 0; index < build->kept_count; index++)
	{
		struct tag_instance const *instance = &build->header->instances[build->kept[index]];
		struct addon_file_tag tag;

		tag.group_tag = (uint32_t)instance->group_tag;
		tag.parent_group_tags[0] = (uint32_t)instance->parent_group_tags[0];
		tag.parent_group_tags[1] = (uint32_t)instance->parent_group_tags[1];
		tag.name_offset = (uint32_t)build->name_offsets[build->kept[index]];
		tag.root_offset = (uint32_t)tag_data_offset(build, (unsigned long)instance->base_address);
		buffer_add(&tags, &tag, sizeof(tag));
	}
	for (index = 0; index < build->import_count; index++)
	{
		struct tag_instance const *instance = &build->header->instances[build->imports[index]];
		struct addon_file_import import;

		import.group_tag = (uint32_t)instance->group_tag;
		import.name_offset = (uint32_t)build->name_offsets[build->imports[index]];
		buffer_add(&imports, &import, sizeof(import));
	}

	memset(&header, 0, sizeof(header));
	header.signature = ADDON_FILE_SIGNATURE;
	header.version = ADDON_FILE_VERSION;
	header.kind = _addon_file_kind_vehicle;
	header.root_tag = (uint32_t)build->indices[root];
	buffer_grow(&file, sizeof(header));
	section_write(&file, &header.tags, tags.bytes, tags.size, sizeof(struct addon_file_tag));
	section_write(&file, &header.imports, imports.bytes, imports.size, sizeof(struct addon_file_import));
	section_write(&file, &header.tag_data, build->tag_data.bytes, build->tag_data.size, 1);
	section_write(&file, &header.relocations, build->relocations.bytes, build->relocations.size, sizeof(uint32_t));
	section_write(&file, &header.import_fields, build->import_fields.bytes, build->import_fields.size,
		sizeof(struct addon_file_import_field));
	section_write(&file, &header.resources, build->resources.bytes, build->resources.size, 1);
	/* (the vertices, then the strips) */
	header.model_index_data_offset = (uint32_t)build->model_vertices.size;
	buffer_add(&build->model_vertices, build->model_strips.bytes, build->model_strips.size);
	section_write(&file, &header.model_data, build->model_vertices.bytes, build->model_vertices.size, 1);
	section_write(&file, &header.seats, build->seats.bytes, build->seats.size, sizeof(struct addon_file_seat));
	section_write(&file, &header.seat_animations, build->seat_animations.bytes, build->seat_animations.size,
		sizeof(struct addon_file_seat_animation));
	header.file_size = (uint32_t)file.size;
	memcpy(file.bytes, &header, sizeof(header));
	*file_size = file.size;

	stream = fopen(path, "wb");
	written = stream && fwrite(file.bytes, 1, file.size, stream) == file.size;
	if (stream)
		written = fclose(stream) == 0 && written;
	free(file.bytes);
	free(tags.bytes);
	free(imports.bytes);
	return written;
}

static void report_print(struct build const *build, char const *vehicle, unsigned long file_size)
{
	long index;
	short state;

	printf("%s: %ld tags kept, %ld imported\n", vehicle, build->kept_count, build->import_count);
	printf("  tag data %lu bytes (%lu pointers, %lu import fields), resources %lu bytes, model data %lu bytes\n",
		build->tag_data.size, build->relocations.size / 4,
		build->import_fields.size / sizeof(struct addon_file_import_field), build->resources.size,
		build->model_vertices.size);
	for (index = 0; index < (long)(build->seats.size / sizeof(struct addon_file_seat)); index++)
	{
		struct addon_file_seat const *seat = (struct addon_file_seat const *)build->seats.bytes + index;

		printf("  seat '%s': %u animations by name\n", seat->label, seat->animation_count);
	}
	if (build->silent_sounds)
		printf("  %ld sounds silent\n", build->silent_sounds);
	if (file_size)
		printf("  file %lu bytes\n", file_size);
	for (state = _tag_kept; state <= _tag_imported; state++)
	{
		printf("%s:\n", state == _tag_kept ? "kept" : "imported");
		for (index = 0; index < build->header->tag_count; index++)
		{
			if (build->states[index] == state)
				printf("  %s %s\n", tag_text(build->header->instances[index].group_tag), tag_name_of(build, index));
		}
	}
}

/* ---------- the map */

/* the Custom Edition map at path loaded, converted and checked into the tag
cache, as the game does (and tools/map_validate.c): 0 if it is refused */
static int map_load(struct build *build, char const *path, char const *resource_folder)
{
	char const *name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
	static struct resource_map storage[NUMBER_OF_RESOURCE_MAP_TYPES];
	struct resource_map *maps[NUMBER_OF_RESOURCE_MAP_TYPES];
	struct custom_edition_conversion_report conversion;
	struct tag_validate_file_range ranges[4];
	short range_count = 0;
	enum cache_file_status status;
	short type;

	if (!memory_file_open(&build->map, path))
	{
		printf("%s: cannot be read\n", name);
		return 0;
	}
	memset(maps, 0, sizeof(maps));
	for (type = _resource_map_bitmaps; type < NUMBER_OF_RESOURCE_MAP_TYPES; type++)
	{
		char resource_path[1024];
		char const *slash = strrchr(path, '/');

		if (resource_folder)
			snprintf(resource_path, sizeof(resource_path), "%s/%s.map", resource_folder,
				resource_map_type_describe((enum resource_map_type)type));
		else
			snprintf(resource_path, sizeof(resource_path), "%.*s/%s.map", slash ? (int)(slash - path) : 1,
				slash ? path : ".", resource_map_type_describe((enum resource_map_type)type));
		if (memory_file_open(&build->resource_files[type], resource_path) &&
			resource_map_open(&build->resource_files[type].source, (enum resource_map_type)type, &storage[type]) ==
				_cache_file_status_ok)
		{
			maps[type] = &storage[type];
		}
	}
	status = custom_edition_cache_load(&build->map.source, maps, build->tag_cache, CUSTOM_EDITION_TAG_CACHE_BYTES,
		&build->report);
	if (status != _cache_file_status_ok)
	{
		printf("%s: its loader refuses it: %s (tag %ld)\n", name, cache_file_status_describe(status),
			(long)build->report.problem_tag_index);
		return 0;
	}
	build->loaded_bytes = build->report.tag_data_bytes + build->report.resource_tag_bytes;
	custom_edition_cache_combine_resource_offsets(build->tag_cache, build->loaded_bytes, CUSTOM_EDITION_BITMAPS_OFFSET,
		CUSTOM_EDITION_SOUNDS_OFFSET);
	/* (the sounds this build would not play decoded and encoded again, before
	the conversion, which silences those it cannot play: as the game does) */
	reading = build;
	quiet = 1;
	custom_edition_sounds_decode(build->tag_cache, build->loaded_bytes, CUSTOM_EDITION_DECODED_OFFSET,
		CUSTOM_EDITION_BITMAPS_OFFSET - CUSTOM_EDITION_DECODED_OFFSET);
	quiet = 0;
	status = custom_edition_cache_convert(build->tag_cache, build->loaded_bytes, build->report.identity.name,
		&conversion);
	if (status != _cache_file_status_ok)
	{
		printf("%s: its conversion refuses it: %s\n", name, cache_file_status_describe(status));
		return 0;
	}
	ranges[range_count].offset = 0;
	ranges[range_count++].size = build->report.identity.file_length;
	ranges[range_count].offset = CUSTOM_EDITION_DECODED_OFFSET;
	ranges[range_count++].size = custom_edition_sounds_decoded_bytes();
	if (maps[_resource_map_bitmaps])
	{
		ranges[range_count].offset = CUSTOM_EDITION_BITMAPS_OFFSET;
		ranges[range_count++].size = build->resource_files[_resource_map_bitmaps].size;
	}
	if (maps[_resource_map_sounds])
	{
		ranges[range_count].offset = CUSTOM_EDITION_SOUNDS_OFFSET;
		ranges[range_count++].size = build->resource_files[_resource_map_sounds].size;
	}
	quiet = 1;
	if (!tag_validate_custom_edition_tags(build->tag_cache, (long)build->loaded_bytes, build->report.tag_cache_bytes,
		ranges, range_count, name))
	{
		quiet = 0;
		printf("%s: the validator refuses it\n", name);
		return 0;
	}
	quiet = 0;
	printf("%s: %ld tags, loaded and checked (%ld corrections)\n", name,
		(long)((struct tag_header *)build->tag_cache)->tag_count, tag_validate_corrections());
	build->header = (struct tag_header *)build->tag_cache;
	return 1;
}

/* ---------- main */

static void usage(void)
{
	fprintf(stderr, "usage: cevehicle_build --map map.map [--maps folder] --vehicle name --riders graph "
		"[--stock map.map]... [--out file.cevehicle] [--report]\n");
}

int main(int argc, char **argv)
{
	static struct build build;
	char const *map_path = NULL;
	char const *resource_folder = NULL;
	char const *vehicle = NULL;
	char const *riders = NULL;
	char const *out = NULL;
	unsigned long file_size = 0;
	int report = 0;
	long root;
	int argument;

	for (argument = 1; argument < argc; argument++)
	{
		if (!strcmp(argv[argument], "--map") && argument + 1 < argc)
			map_path = argv[++argument];
		else if (!strcmp(argv[argument], "--maps") && argument + 1 < argc)
			resource_folder = argv[++argument];
		else if (!strcmp(argv[argument], "--vehicle") && argument + 1 < argc)
			vehicle = argv[++argument];
		else if (!strcmp(argv[argument], "--riders") && argument + 1 < argc)
			riders = argv[++argument];
		else if (!strcmp(argv[argument], "--out") && argument + 1 < argc)
			out = argv[++argument];
		else if (!strcmp(argv[argument], "--stock") && argument + 1 < argc && build.stock_count < MAXIMUM_STOCK_MAPS)
		{
			if (!stock_map_read(&build.stock[build.stock_count], argv[++argument]))
			{
				fprintf(stderr, "cevehicle_build: %s is not an Xbox map\n", argv[argument]);
				return 2;
			}
			build.stock_count++;
		}
		else if (!strcmp(argv[argument], "--report"))
			report = 1;
		else
		{
			usage();
			return 2;
		}
	}
	if (!map_path || !vehicle || !riders)
	{
		usage();
		return 2;
	}
	build.tag_cache = mmap((void *)CUSTOM_EDITION_TAG_CACHE_ADDRESS, CUSTOM_EDITION_TAG_CACHE_BYTES,
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
	if (build.tag_cache != (unsigned char *)CUSTOM_EDITION_TAG_CACHE_ADDRESS)
	{
		fprintf(stderr, "cevehicle_build: the Custom Edition tag cache's address (%08lx) is taken\n",
			(unsigned long)CUSTOM_EDITION_TAG_CACHE_ADDRESS);
		return 2;
	}
	if (!map_load(&build, map_path, resource_folder) || !vehicle_walk(&build, vehicle, &root))
		return 1;
	tag_data_write(&build);
	bitmaps_write(&build);
	sounds_write(&build);
	models_write(&build);
	seats_write(&build, root, riders);
	if (build.problems)
	{
		printf("%s: %ld problems: no file written\n", vehicle, build.problems);
		return 1;
	}
	if (out && !file_write(&build, root, out, &file_size))
	{
		fprintf(stderr, "cevehicle_build: cannot write %s\n", out);
		return 1;
	}
	if (report)
		report_print(&build, vehicle, file_size);
	return 0;
}
