# Mutators

A mutator changes the rules of a game: it turns on effects that are built
into the game, and sets their parameters. Mods (`docs/mods.md`) change only
what one machine plays and shows. A mutator changes what every machine of a
network game must agree on, so a game plays the host's mutators on every
machine.

The game's code is `port/linux/game/mutators.c` (the effects and their
parameters), `mutator_big_head.c` (the big head), `mutator_tags.c` (the tag
edits), `network_mutators.c` (the host's mutators sent to its clients) and
`port/linux/src/port_mutators.c` (the mutators' files).

## What a mutator can do

A mutator is only data. It names effects that the game has built in, and
gives values to their parameters; and it sets values of the map's tags, of
the fields that the game lists (the tag edits, below). Nothing in a mutator
runs. The game refuses a mutator that names an effect, a parameter, a key or
a field that it does not have, or a value outside the range of its parameter
or field. So a mutator can do no more than the built-in effects do, and than
a map built with those values of those fields would.

| Effect | Parameter | Range | Default | What it does |
| --- | --- | --- | --- | --- |
| `big_head` | `scale` | 1 to 4 | 2.5 | Every biped's head, the players' and the AI's, is drawn and hit at `scale` times its size. |

### Big head

The game scales the node of each biped's head (the biped's own head node)
before it makes the matrices of the biped's nodes. The model is drawn with
those matrices, and the collision model is tested with them, so a shot hits
the head where it is drawn, on every machine. Markers on the head node move
with it. The spheres that the game looks for an object in (shots, melee,
explosions, the host's check of a client's hit) and that it culls an object
by grow to hold the larger head.

## Tag edits

A mutator's `[[tags]]` tables each name a tag of the map, by its group and
its name, and set some of its fields:

```toml
[[tags]]
group = "weapon"                              # the tag's group (below)
tag = 'weapons\assault rifle\assault rifle'    # its name (a literal string: no escapes)
zoom_level_count = 1                          # a field: a number
zoom_in_sound = 'sound\sfx\weapons\sniper rifle\sniper_2x_zoom'   # a tag's name ('' for none)

[tags.triggers.0]                             # an element of a block: its index
projectile_error_angle_upper_bound = 3        # (angles in degrees)

[[tags]]
group = "projectile"
tag = 'weapons\assault rifle\bullet'
[tags.material_responses.stone]               # ... or its name
potential_response = "reflect"                # one of the field's names

[[tags]]
group = "weapon_hud_interface"
tag = 'weapons\assault rifle\assault rifle'
valid_crosshair_types = ["aim", "zoom"]       # some of the field's names
crosshairs = { from = 'weapons\pistol\pistol', elements = [1, 2], append = true }
```

A copy (`from`) puts the elements of the same block of another tag of the
group in place of the block's own: all of them, or those `elements` lists,
and after the block's own with `append = true`.

The edits are made when the map's tags have loaded, before any of its
objects: by the host, and by a game of one machine, as each game starts, and
by a client when the host's come. A tag, an element or a tag named that the
map does not have is passed over and logged (`debug.txt`), and the console
says how many were; the other edits are made. Every machine of a game plays
one map, so each passes over the same. The next game puts back the map's own
values before it makes its own edits.

The fields that a mutator can set:

| Group | Field | Value |
| --- | --- | --- |
| `weapon` | `zoom_level_count` | 0 to 8 |
| | `zoom_magnification_minimum`, `zoom_magnification_maximum` | 1 to 32 |
| | `zoom_in_sound`, `zoom_out_sound` | a sound's name |
| | `triggers.<index>.projectile_error_angle_lower_bound`, `..._upper_bound` | 0 to 180 degrees |
| | `triggers.<index>.rounds_between_tracers` | 0 to 32767 |
| `projectile` | `maximum_range` | 0 to 10000 world units |
| | `material_responses.<material>.default_response`, `.potential_response` | `disappear`, `detonate`, `reflect`, `overpenetrate`, `attach` |
| | `material_responses.<material>.default_effect`, `.potential_effect` | an effect's name |
| | `material_responses.<material>.potential_flags` | `only_against_units` |
| | `material_responses.<material>.potential_skip_fraction` | 0 to 1 |
| | `material_responses.<material>.potential_angle_lower_bound`, `..._upper_bound` | -90 to 90 degrees |
| `weapon_hud_interface` | `valid_crosshair_types` | `aim`, `zoom`, `charge`, `flash_ammo`, ... (`mutator_tags.c` lists them) |
| | `crosshairs` | copied, 19 elements at most |
| | `screen_effects` | copied, 1 element at most |

A material is one of `dirt`, `sand`, `stone`, `snow`, `wood`, `hollow_metal`,
`thin_metal`, `thick_metal`, `rubber`, `glass`, `force_field`, `grunt`,
`hunter_armor`, `hunter_skin`, `elite`, `jackal`, `jackal_energy_shield`,
`engineer`, `engineer_force_field`, `flood_combat_form`,
`flood_carrier_form`, `cyborg`, `cyborg_energy_shield`, `armored_human`,
`human`, `sentinel`, `monitor`, `plastic`, `water`, `leaves`,
`elite_energy_shield`, `ice` or `hunter_shield`.

A mutator with tag edits is made for `format = 2`. Limits: 32 KB of edits
in a mutator and in a game (an edit is its names and a few bytes), and 2048
edits made in a game.

### Accurized MA5B

The assault rifle zooms, with the pistol's 2x scope (its crosshairs and its
mask, so a map with the pistol needs nothing more), and the sniper rifle's
zoom sounds. Its bullets spread from 0.05 to 3 degrees (the stock rifle's 2
to 6.5), build up and settle in the stock rifle's times (0.6 and 1 second),
and a tracer is every fifth round, not every third. They carry 300 world
units (the stock rifle's 40), at the stock speed and with its gravity, and
ricochet as Combat Revolved's do: off stone, metal and ice at up to 60
degrees from the surface, one time in ten, and always off energy shields,
force fields and a hunter's armor, which still take the bullet's damage.
They pass through flesh as the stock rifle's do. The rest of the rifle (its
damage, rate of fire, magazine, flashlight) is stock.

On a map without the pistol (some campaign levels), the rifle zooms without
the scope's crosshairs and mask; on one without the sniper rifle's zoom
sounds, silently.

## The mutators

The game has the mutators built into it (`big_head`, `accurized_ma5b`), and the mutators in
the `mutators` folder of the data root (the folder with `maps/`). Each
mutator is a folder with a `mutator.toml`:

```
mutators/
  huge_heads/
    mutator.toml
```

```toml
name = "Huge heads"     # what the Mods menu shows (else the folder's name)
id = "huge_heads"       # what game.mutators names it by (else the folder's name)
version = "1.0"
authors = ["Someone"]   # or one string
description = "Heads at four times their size."
format = 1              # the mutators' format it is made for

[effects.big_head]      # an effect it turns on
scale = 4               # its parameters (else their defaults)
```

Every key but `effects` or `tags` is optional. A mutator cannot be played,
and the Mods menu says why, if:

- `mutator.toml` is not TOML, or is larger than 64 KB;
- it has a key that the game does not know (at the top or in `effects`);
- it names no effect and edits no tag, or names an effect that the game does
  not have, or a value out of its parameter's range;
- it edits a group, a field or an element (by its name) that the game does
  not edit, with a value that is not of the field's kind or is out of its
  range;
- it is made for a later `format`;
- another mutator, found before it, has its `id` (not case-sensitive), or its
  `id` has a comma, a quote or a backslash.

Limits: 64 mutators, folder names of 63 bytes at most, 16 effects and
parameters in a mutator.

On the web, only the mutators built into the game are found: the page cannot
add a mutators folder yet.

## Turning them on

The main menu's **Mods**, then **MUTATORS**, shows the mutators in the rows
in place of the mods (built-in first, then the folders by name). **MODS**
shows the mods again. A turns the chosen mutator on or off; the details say
what it sets and why it cannot be played, if it cannot. **OK** keeps the
mods and the mutators, **Cancel** forgets both.

The mutators turned on are `game.mutators` in `config.toml` (their ids,
separated by commas; `HALO_MUTATORS` for one run). Empty, the default, plays
none: the stock game.

Where two mutators that are on set one parameter, the one found later sets
it.

## Network games

- The host, and a game of one machine, plays the mutators that are on when
  the map loads. Changes take effect in the next game.
- A client plays the host's mutators, never its own. As each client is ready
  for the host's game, before the host's objects, the host sends it each
  effect's and parameter's key and value (distributed message 112, network
  version 39).
- A client takes them only from the host, reliably, and only whole: a count
  that the size does not match, a key that is not a key's text, or a value
  outside the range of a key that it has leaves the message unplayed.
- A key that the client's build does not have (a fork's own effect) is passed
  over, and the console says so: that game cannot play as the host's does.
- The host's tag edits go before the keys (distributed message 113, network
  version 40): their bytes in pieces of 4000, each its own reliable message.
  A client takes a piece only from the host, and only where the last ended,
  and makes the edits when it has them all. Edits of a group, field or value
  that its build does not take leave none made, and the log says so.
- Each machine's console and `debug.txt` name the mutators of the game.
