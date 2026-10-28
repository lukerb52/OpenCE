# Add-ons

An add-on is a vehicle, and every tag it uses, that a mutator adds to the
maps it is played on: its model, collision, physics, animations, shaders,
textures, sounds, effects, weapon and HUD, and the seats its riders sit in.
It is a `.cevehicle` file, made from a Halo Custom Edition map by the
add-ons' builder, and a mutator names it beside its `mutator.toml`, with the
places on each map where its vehicle is placed. The map's own tags and
vehicles stay as they are: the add-on's are added after them.

The game's code is `port/linux/game/addons.c` (the add-ons' tags added to a
map's), `port/linux/game/addon_file.h` (the file), `tag_walk.c` (a tag's
fields walked through its group's schema), `custom_edition_geometry.c` (an
add-on's models, as a Custom Edition map's) and
`port/linux/src/port_mutators.c` (`addon` and `[[place]]`). The builder is
`tools/cevehicle_build.c`.

## A mutator with an add-on

```
mutators/
  falcon/
    mutator.toml
    falcon.cevehicle
```

```toml
name = "Falcon"
id = "falcon"
format = 3                  # an add-on needs format 3
addon = "falcon.cevehicle"  # a file beside mutator.toml: a name, no folder

[[place]]                   # one for each vehicle placed (16 at most)
map = "bloodgulch"          # the map's file's name
vehicle = 'vehicles\falcon\falcon'  # the vehicle's tag (the add-on's, or the map's)
position = [35.750, -98.335, 1.0]   # world units
facing = -48.6              # degrees (else 0)
```

A mutator cannot be played if its `addon` names a file that is not beside
its `mutator.toml`, or one larger than 64 MB; if it has places but no
add-on; or if a place has a key the game does not have, or lacks its map,
vehicle or position. A stock mutator has no add-on. A map with no places is
given the add-on's tags but none of its vehicles.

A vehicle is placed as the scenario's first vehicle is (its permutation, the
bsps it is on) where its place says, and added to the scenario's vehicle
palette. This build chooses a multiplayer game's vehicles by type
(`game_engine_remap_vehicle`): an add-on's vehicle, which no vehicle set
names, is kept unless the game variant has no vehicles. It respawns as the
map's vehicles do. A scenario has room for 80 vehicles.

`addons/falcon` in this repository is the Falcon of `bigass_open_v1.1.map`,
kept until it has a repository of its own: copy its folder, with the file
built from the map (below), into the data root's `mutators` folder.

## Making an add-on

```sh
ninja linux
build/linux/cevehicle_build --map <custom_maps>/bigass_open_v1.1.map --maps <custom_maps> \
    --vehicle 'vehicles\falcon\falcon' --riders 'characters\cyborg_mp\bigass with grackle' \
    --stock <maps>/beavercreek.map ... --stock <maps>/wizard.map \
    --out falcon.cevehicle --report
```

- `--map` is the Custom Edition map the vehicle is in, and `--maps` the
  folder of its `bitmaps.map`, `sounds.map` and `loc.map` (else beside it).
  The map is read, converted and checked as the game reads one
  (`custom_edition_cache.c`).
- `--vehicle` is the vehicle's tag. Every tag it names, and every tag those
  name, is the add-on's, unless every map given by `--stock` (an Xbox map,
  each of the multiplayer maps the add-on is for) has a tag of its group and
  name: then the add-on names the map's own, an import. A map that lacks an
  import cannot be given the add-on.
- `--riders` is the animation graph, in the map, of the bipeds that ride
  it. Each seat label of the vehicle's seats is written from it, its
  animations by name.
- `--report` lists the tags kept and imported, the sizes, the seats, and the
  sounds the game cannot play.

The builder makes the same file from the same map every time. Nothing in it
is run: it is the tags, their pixels, samples and geometry, and the lists
the game needs to place them.

## The file

`port/linux/game/addon_file.h` lays it out: a header, then sections.

| Section | What it is |
| --- | --- |
| Tags | Each of the add-on's tags: its groups, and its name and root in the tag data |
| Imports | Each tag of the map's that it names: its group and name |
| Tag data | The tags' names, then their roots, blocks and data, as the Custom Edition loader leaves them once it has converted them: this build's layouts, but for gbxmodels |
| Relocations | Where each pointer in the tag data is; each holds an offset in the tag data |
| Import fields | Where a reference or tag index names an import (it holds none until the game gives it the map's tag) |
| Resources | The bitmaps' pixels and the sounds' samples (in Xbox ADPCM: the builder decodes Halo PC's Ogg Vorbis and other sounds as the game does a Custom Edition map's) |
| Model data | The gbxmodels' parts' uncompressed vertices, then their strips |
| Seats, seat animations | Each seat label: its element of an animation graph's unit seats, and each animation index in it with the name of its animation |

## How a map is given an add-on

When a map's tags load and pass the validator (`cache_files.c`), before
the menus' tags are added (`menu_tags.c`), each add-on of the mutators on
is read:

1. Every section must lie in the file, every relocation in the tag data and
   every pointer it names within it.
2. Its tags are laid out as a Custom Edition map's (a tag header, the
   table, the tag data) and checked by the validator as one's are
   (`tag_validate_addon_tags`), apart from the map's: the map's own checks,
   and those of its structure bsps as they load, are as they were.
3. Each import must be a tag of the map's (a gbxmodel's import is a model).
4. The add-on's tags join the map's table after its own, in the order of
   the mutators on, so every machine with the same add-ons gives them the
   same indices. A tag of the add-on's whose group and name the map has is
   the map's: the game finds a reference's tag by its name.
5. Its tags are walked through their groups' schemas (`tag_walk.c`), and
   each reference and tag index is given its tag's index in the table (an
   import field the map's tag; a field the file names that is not a
   reference or a tag index refuses the add-on). Its gbxmodels are made this
   build's models, their geometry compressed into buffers of their own
   (`custom_edition_addon_models_convert`). Its bitmaps and sounds name
   their tags, and their pixels and samples are read from the file
   (`addons_read`, from `cache_file_read`).
6. Each seat label is added to the animation graphs of the map's bipeds that
   lack it, each animation found by its name in the graph: a graph without
   one refuses the seat, and the log says which.
7. Its vehicles are placed.

An add-on that fails any of these is not added, and `debug.txt` says why.
All of it is taken back before the map's tags go. The main menu's map is
given none.

`debug.network_test_vehicle` seats the player in an add-on's vehicle when
there is one, and the network test logs each add-on vehicle's position,
speed and riders every second.

## Network games

Network version 43. Each machine adds the add-ons of its own mutators that
are on; the host does not send its add-ons yet, so every machine of a game
must have the same add-ons on (the same files: the builder makes the same
bytes from the same map). A client without them plays a map without the
host's vehicles: what it sees of them, and of the players in them, is not
the host's.

## What is not done yet

- The host sending its add-ons to its clients, as it sends a map.
- Importing a mutator from a file or a catalog in the game.
- Custom Edition maps are given no add-ons; the web build finds only the
  stock mutators.
- A bitmap that Halo PC draws linear whose rows are not whole steps of the
  pitch is not drawn; the HUD's bitmaps and meters are drawn as an Xbox
  map's are, not as Halo PC draws them.
- Making an add-on from tag files (with Invader building a map of the
  vehicle alone, which the builder reads as it reads any).
