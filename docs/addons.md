# Add-ons

An add-on is a vehicle or a weapon, and every tag it uses, that a mutator
adds to the maps it is played on: its model, collision, physics, animations,
shaders, textures, sounds, effects, projectiles and HUD, and for a vehicle
the seats its riders sit in. It is a `.cevehicle` or `.ceweapon` file, made
from a Halo Custom Edition map by the add-ons' builder, and a mutator names
it beside its `mutator.toml`, with the places on each map where its vehicle
is placed or its weapon spawns. The map's own tags, vehicles and weapons stay
as they are: the add-on's are added after them, and never replace the stock
game's (a change to a stock weapon is a mutator's tag edits,
[mutators.md](mutators.md)).

The game's code is `port/linux/game/addons.c` (the add-ons' tags added to a
map's), `port/linux/game/addon_file.h` (the file), `tag_walk.c` (a tag's
fields walked through its group's schema), `custom_edition_geometry.c` (an
add-on's models, as a Custom Edition map's) and
`port/linux/src/port_mutators.c` (`addon` and `[[place]]`). The builder is
`tools/addon_build.c`.

## A mutator with an add-on

```
mutators/
  falcon/
    mutator.toml
    falcon.cevehicle
  dmr/
    mutator.toml
    dmr.ceweapon
```

```toml
name = "Falcon"             # what the menus call its vehicle (or weapon)
id = "falcon"
format = 3                  # a vehicle needs format 3, a weapon 4
addon = "falcon.cevehicle"  # a file beside mutator.toml: a name, no folder

[[place]]                   # one for each vehicle or weapon (16 at most)
map = "bloodgulch"          # the map's file's name
vehicle = 'vehicles\falcon\falcon'  # the vehicle's tag (the add-on's, or the map's)
position = [35.750, -98.335, 1.0]   # world units
facing = -48.6              # degrees (else 0)
```

```toml
name = "DMR"
id = "dmr"
format = 4
addon = "dmr.ceweapon"

[[place]]
map = "bloodgulch"
weapon = 'bourrin\weapons\dmr\dmr'  # the weapon's tag (the add-on's)
position = [31.0, -88.0, 0.5]
```

An add-on's kind is its file's name's: `.cevehicle` or `.ceweapon` (any
case), which the game checks against the file. A mutator cannot be played if
its `addon` names a file that is not beside its `mutator.toml`, one larger
than 64 MB, or one of another name; if it has places but no add-on; or if a
place has a key the game does not have, names both a vehicle and a weapon, or
lacks its map, its vehicle or weapon, or its position. A stock mutator has
no add-on. A map with no places is given the add-on's tags but none of its
vehicles or weapons.

### Vehicles

A vehicle is placed as the scenario's first vehicle is (its permutation, the
bsps it is on) where its place says, and added to the scenario's vehicle
palette. It respawns as the map's vehicles do. A scenario has room for 80
vehicles. Each team's vehicle set (Server Setup's VEHICLE OPTIONS) decides
whether it is placed, on the side whose spawn points are nearest:

| Vehicle set | An add-on's vehicles |
| --- | --- |
| DEFAULT, PC | placed |
| NONE, and the sets of one kind (WARTHOGS, GHOSTS, ...) | left out |
| CUSTOM | the first two vehicle add-ons of the mutators on have rows of their own, after GUN TURRET (their mutators' names: NONE to 4, 4 at first); the rest are placed |

### Weapons

A weapon spawns where its place says as the map's netgame equipment spawns
items, by the item collection the builder took with it (one of the map's
that spawns it alone): in every game type, again when it has been taken (the
collection's spawn time, else 30 seconds), not when the gametype has no
map weapons. The weapon sets (ITEM OPTIONS' WEAPON SET) keep it in NORMAL and
NO SNIPING, which have weapons of every kind, and leave it out of the rest,
which are of one kind. A scenario has room for 200 netgame items.

A custom loadout (LOADOUT: CUSTOM) lists the weapon add-ons of the mutators
on after the stock weapons, by their mutators' names: each player starts
with the one chosen, where the map has it (8 at most; one whose mutator is
off gives none).

A unit holds a weapon by its label, a weapon type its animation graph has
animations for (the assault rifle's is `ar`), and picks up only one its graph
has: the log says so of a weapon of a type the map's bipeds' graphs lack.

`addons/falcon` in this repository is the Falcon of `bigass_open_v1.1.map`,
kept until it has a repository of its own: copy its folder, with the file
built from the map (below), into the data root's `mutators` folder.

## Making an add-on

```sh
ninja linux
build/linux/addon_build --map <custom_maps>/bigass_open_v1.1.map --maps <custom_maps> \
    --vehicle 'vehicles\falcon\falcon' --riders 'characters\cyborg_mp\bigass with grackle' \
    --stock <maps>/beavercreek.map ... --stock <maps>/wizard.map \
    --out falcon.cevehicle --report
build/linux/addon_build --map <custom_maps>/bigass_open_v1.1.map --maps <custom_maps> \
    --weapon 'bourrin\weapons\dmr\dmr' \
    --stock <maps>/beavercreek.map ... --stock <maps>/wizard.map \
    --out dmr.ceweapon --report
```

- `--map` is the Custom Edition map the vehicle or weapon is in, and
  `--maps` the folder of its `bitmaps.map`, `sounds.map` and `loc.map` (else
  beside it). The map is read, converted and checked as the game reads one
  (`custom_edition_cache.c`).
- `--vehicle` or `--weapon` is its tag. Every tag it names, and every tag
  those name, is the add-on's, unless every map given by `--stock` (an Xbox
  map, each of the multiplayer maps the add-on is for) has a tag of its group
  and name: then the add-on names the map's own, an import. A map that lacks
  an import cannot be given the add-on.
- `--riders` (a vehicle's) is the animation graph, in the map, of the bipeds
  that ride it. Each seat label of the vehicle's seats is written from it,
  its animations by name.
- A weapon takes with it the map's item collections that spawn it alone,
  which its places spawn it by (a weapon of none cannot be placed), and the
  map's HUD globals' messaging icons: its pickup's picture is one of them,
  which the game draws from the add-on's, not the playing map's.
- `--hud-parent` (a weapon's) draws its HUD in a stock HUD's frame, at the
  top left: the weapon's HUD interface's parent becomes that HUD interface of
  the stock maps' (an import: `'ui\hud\master rounds'` is the stock weapons'
  round counter and warnings), and its own statics and numbers, drawn for its
  map's HUD, are left out; its meters and crosshairs stay. `--hud-meter x,y`
  moves its first meter (its rounds) in the frame (the pistol's are at 4,20).
  `--hud-keep name` keeps the weapon's HUD interface's parents down to that
  one (a parent that draws its reticle), whose parent becomes the stock one,
  and `--hud-strip name` (up to 4) leaves out a kept one's statics and
  numbers. `--hud-meter-reverse` makes its first meter empty from the other
  end: the fill order in its sprite's colors (an Xbox-order meter's, a 32-bit
  ARGB bitmap of the add-on's) reflected from its least to its greatest, in
  the add-on's pixels only, its shapes as they were. The add-on records its map's Halo PC behaviours, which its
  bitmaps' channels follow where it plays (bigass_open_v1.1's ammo meters
  keep the Xbox's channels).
- `--report` lists the tags kept and imported, the sizes, the seats, a
  weapon's label and item collections, and the sounds the game cannot play.

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
| Icon texts, texts | Each object's and seat's icon text index (what a player getting in is told the vehicle and seat are), and its text, from the map's HUD globals' alternate icon text |

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
   (`addons_read`, from `cache_file_read`; a bitmap's by
   `addons_bitmaps_read`, from the texture cache). Its bitmaps are Halo PC's
   and are drawn as a Custom Edition map's are: their pixels laid out again
   as they arrive, the channels of its HUD meters and model shaders' masks
   found where Halo PC keeps them (`custom_edition_bitmaps.c`), and the HUD
   draws its meters, linear maps and additive overlays as Halo PC does.
6. Each icon text is found in the map's HUD globals' alternate icon text,
   which is given those it lacks ("Falcon"), and its index written. Each
   seat label is added to the animation graphs of the map's bipeds that
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
  pitch is not drawn. The Halo PC behaviours a map relies on (Chimera's map
  list) are its own: an add-on taken from one does not bring them.
- Making an add-on from tag files (with Invader building a map of the
  vehicle alone, which the builder reads as it reads any).
