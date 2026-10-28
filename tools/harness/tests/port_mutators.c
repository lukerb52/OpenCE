/*
The mutators' files (port/linux/src/port_mutators.c), whole, over a fake
mutators folder: the stock mutators written into stock_mutators (again when
changed, not when unchanged) and played as the game has them (also when they
cannot be written), mutator.toml read and refused, the order, the ids
game.mutators turns on, and what the Mods menu keeps.
*/

#include "harness.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
/* (the library is C11: the harness builds C99) */
#define static_assert _Static_assert
#include "tomlc17.inc"
#pragma clang diagnostic pop

/* the folder: each entry a folder (with its mutator.toml's text, or NULL:
none) */
struct fake_folder { char const *name; char const *text; unsigned long size; };
static struct fake_folder folders[8];
static int folder_count, listed, reads, logged;
static char setting[300], written[300];

#include "port_mutators_source.inc"

static void folder(char const *name, char const *text)
{
	folders[folder_count].name = name;
	folders[folder_count].text = text;
	folders[folder_count++].size = text ? (unsigned long)strlen(text) : 0;
}

/* whether the file written at `path` has `text` in it */
static int written_holds(char const *path, char const *text)
{
	int index;

	for (index = 0; index < 16; index++)
	{
		if (files_written[index].text && !strcmp(files_written[index].path, path))
			return strstr(files_written[index].text, text) != NULL;
	}
	return 0;
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

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	char keys[16][PORT_MUTATOR_KEY_LENGTH];
	double values[16];
	char text[1024];

	CHECK(argc == 2, "case");
	if (!strcmp(case_name, "stock"))
	{
		size_t size = 0;
		char *readme;

		CHECK(port_mutators_scan() == 0, "a mutator on with game.mutators empty");
		CHECK(port_mutators_count() == 3 && find("accurized_ma5b") == 0 && find("big_head") == 1 &&
			find("revolved_plasma_rifle") == 2, "the stock mutators, by folder");
		CHECK(file_writes == 4, "the README and the three mutator.toml written (%d)", file_writes);
		CHECK(written_holds("/data/stock_mutators/big_head/mutator.toml", "[effects.big_head]\nscale = 2.5\n") &&
			written_holds("/data/stock_mutators/accurized_ma5b/mutator.toml", "id = \"accurized_ma5b\"") &&
			written_holds("/data/stock_mutators/revolved_plasma_rifle/mutator.toml", "group = \"damage_effect\""),
			"the stock mutators' files");
		readme = config_file_read("/data/stock_mutators/README.txt", &size);
		CHECK(readme && strstr(readme, "copy one of"), "the README");
		free(readme);
		port_mutators_scan();
		CHECK(file_writes == 4, "files that hold the game's written again (%d)", file_writes);
		/* (written after the README and accurized_ma5b's) */
		CHECK(!strcmp(files_written[2].path, "/data/stock_mutators/big_head/mutator.toml"), "the order written");
		strcpy(files_written[2].text, "id = \"changed\"\n[effects.big_head]\nscale = 4\n");
		port_mutators_scan();
		CHECK(file_writes == 5 && written_holds(files_written[2].path, "format = 1"), "a changed file not put back");
		CHECK(find("changed") == NONE, "a changed file played");
		CHECK(port_mutators_status(0, text, sizeof(text)) == _port_mutator_fine, "%s", text);
		CHECK(port_mutators_status(1, text, sizeof(text)) == _port_mutator_fine, "%s", text);
		CHECK(port_mutators_status(2, text, sizeof(text)) == _port_mutator_fine, "%s", text);
		CHECK(port_mutators_keys(keys, values, 16) == 0, "keys of a mutator off");
		strcpy(setting, " Big_Head ");
		CHECK(port_mutators_scan() == 1 && port_mutators_on(1), "game.mutators's id (any case, spaced)");
		CHECK(port_mutators_keys(keys, values, 16) == 2 && !strcmp(keys[0], "big_head") && values[0] == 1.0 &&
			!strcmp(keys[1], "big_head.scale") && values[1] == 2.5, "the big head's keys");
		strcpy(setting, "big_head_x,xbig_head");
		CHECK(port_mutators_scan() == 0, "an id taken for another it begins or ends");
		CHECK(port_mutators_details(1, text, sizeof(text)) && strstr(text, "stock_mutators/big_head") &&
			strstr(text, "scale 2.5"), "the details: %s", text);
	}
	else if (!strcmp(case_name, "stock_unwritable"))
	{
		files_unwritable = 1;
		CHECK(port_mutators_scan() == 0 && port_mutators_count() == 3, "the stock mutators unwritten not played");
		CHECK(port_mutators_status(0, text, sizeof(text)) == _port_mutator_fine &&
			port_mutators_status(2, text, sizeof(text)) == _port_mutator_fine, "%s", text);
		CHECK(logged == 1, "the folder unwritten logged (%d)", logged);
		port_mutators_scan();
		CHECK(logged == 1, "the folder unwritten logged again (%d)", logged);
		strcpy(setting, "big_head");
		CHECK(port_mutators_scan() == 1 && port_mutators_on(1), "a stock mutator unwritten turned on");
	}
	else if (!strcmp(case_name, "folders"))
	{
		static char large[70000];

		memset(large, '#', sizeof(large) - 1);
		folder("zeta", "name = \"Huge heads\"\n[effects.big_head]\nscale = 4\n");
		folder("alpha", "id = \"moon\"\n[effects.moon_gravity]\nscale = 0.2\n");
		folder("broken", "this is [not toml");
		folder("copy", "id = \"BIG_HEAD\"\n[effects.big_head]\n");
		folder("none", NULL);
		folder("range", "[effects.big_head]\nscale = 40\n");
		folder("large", large);
		folder("unknown", "script = \"run()\"\n[effects.big_head]\n");
		strcpy(setting, "zeta,moon,big_head,copy,range,large,unknown");
		port_mutators_scan();
		CHECK(port_mutators_count() == 10, "the folders with a mutator.toml, and the stock mutators (%ld)",
			port_mutators_count());
		CHECK(find("accurized_ma5b") == 0 && find("big_head") == 1 && find("revolved_plasma_rifle") == 2 &&
			find("moon") == 3 && find("broken") == 4 && find("BIG_HEAD") == 5 && find("large") == 6 && find("range") == 7 &&
			find("unknown") == 8 && find("zeta") == 9, "the stock mutators first, then the folders by name");
		CHECK(reads == 6, "the large file read, or one without mutator.toml (%d)", reads);
		CHECK(port_mutators_status(find("moon"), text, sizeof(text)) == _port_mutator_unplayable &&
			strstr(text, "moon_gravity"), "an effect this build lacks: %s", text);
		CHECK(port_mutators_status(find("broken"), text, sizeof(text)) == _port_mutator_unplayable, "not TOML");
		CHECK(logged >= 7, "the refusals and the folder without mutator.toml not logged (%d)", logged);
		CHECK(port_mutators_status(find("BIG_HEAD"), text, sizeof(text)) == _port_mutator_unplayable &&
			strstr(text, "one of its own"), "another's id (any case): %s", text);
		CHECK(port_mutators_details(find("zeta"), text, sizeof(text)) && strstr(text, "Folder: mutators/zeta"),
			"a folder's details: %s", text);
		CHECK(port_mutators_status(find("range"), text, sizeof(text)) == _port_mutator_unplayable &&
			strstr(text, "from 1 to 4"), "out of range: %s", text);
		CHECK(port_mutators_status(find("large"), text, sizeof(text)) == _port_mutator_unplayable, "64 KB");
		CHECK(port_mutators_status(find("unknown"), text, sizeof(text)) == _port_mutator_unplayable &&
			strstr(text, "script"), "a key this build does not know: %s", text);
		CHECK(!port_mutators_on(find("moon")) && !port_mutators_on(find("range")), "a refused mutator on");
		CHECK(port_mutators_keys(keys, values, 16) == 4 && !strcmp(keys[0], "big_head") && values[1] == 2.5 &&
			!strcmp(keys[3], "big_head.scale") && values[3] == 4.0, "the keys of those that can be played, in order");
		CHECK(!strcmp(port_mutators_name(find("zeta")), "Huge heads"), "the name");
		CHECK(port_mutators_keys(keys, values, 3) == 3, "the most keys asked for");
	}
	else if (!strcmp(case_name, "menu"))
	{
		folder("zeta", "[effects.big_head]\nscale = 3\n");
		folder("bad", "[effects.nothing]\n");
		strcpy(setting, "zeta");
		port_mutators_scan();
		port_mutators_turn(find("big_head"), 1);
		port_mutators_turn(find("zeta"), 0);
		port_mutators_turn(find("bad"), 1);
		CHECK(port_mutators_save() && !strcmp(written, "big_head"), "kept: %s", written);
		port_mutators_turn(find("zeta"), 1);
		port_mutators_revert();
		CHECK(!port_mutators_on(find("zeta")) && port_mutators_on(find("big_head")), "reverted to what was kept");
		port_mutators_turn(find("zeta"), 1);
		CHECK(port_mutators_save() && !strcmp(written, "big_head,zeta"), "kept: %s", written);
		port_mutators_turn(-1, 1);
		port_mutators_turn(99, 1);
		CHECK(!port_mutators_on(-1) && !port_mutators_details(99, text, sizeof(text)), "an index past the list");
	}
	else
	{
		CHECK(0, "no case %s", case_name);
	}
	return 0;
}
