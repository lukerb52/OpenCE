"""Mutators' native Halo Script (.hsc) compiled by the game itself (halo --check-script, port/linux/game/mutator_scripts.c):
what the host API takes compiles, what the sandbox refuses does not. Needs ninja linux's build/linux/halo, no assets."""
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
GAME = ROOT / "build/linux/halo"


def run_game(tmp_path, script):
    if not GAME.is_file():
        pytest.skip("needs ninja linux's build/linux/halo")
    return subprocess.run([str(GAME), "--config", str(tmp_path / "config.toml"), "--check-script", str(script)],
                          capture_output=True, text=True, timeout=60, cwd=tmp_path,
                          env={"HOME": str(tmp_path), "XDG_DATA_HOME": str(tmp_path)})


def check(tmp_path, source):
    """(exit code, the game's last line) for the script's text"""
    script = tmp_path / "main.hsc"
    script.write_text(source)
    run = run_game(tmp_path, script)
    lines = [line for line in (run.stdout + run.stderr).splitlines() if not line.startswith("halo-linux")]
    return run.returncode, lines[-1] if lines else ""


COMPILES = {
    "lifecycle": "(global long frames 0)\n"
                 "(script static void startup (set frames 0))\n"
                 "(script static void frame (set frames (+ frames 1)))\n"
                 "(script static void stop (set frames 0))",
    # a host API name the engine also has (game_time) is the API's
    "host_api_first": "(global long last 0)\n(script static void tick (set last (game_time)))",
    "helper_scripts": "(global real r 0)\n"
                      "(script static void helper (set r (math_sqrt 2)))\n"
                      "(script static void frame (helper))",
    "settings_and_state": "(script static void player_tick\n"
                          "    (state_set_real (game_player_current) 0 (settings_real \"range\")))",
}

REFUSED = {
    "unknown_function": "(script static void frame (no_such_function 1))",
    # an engine function outside the sandbox's list
    "engine_function": "(script static void frame (object_create \"x\"))",
    "map_global": "(script static void frame (set cheat_deathless_player true))",
    "wrong_argument_type": "(script static void frame (math_sqrt \"two\"))",
    # a quoted name is the global of that name when there is one: the setting
    # "speed" became the real global speed
    "string_shadowed_by_global": "(global real speed 0)\n"
                                 "(script static void frame (set speed (settings_real \"speed\")))",
    "unbalanced": "(script static void frame (begin)",
}


@pytest.mark.parametrize("name", sorted(COMPILES))
def test_compiles(tmp_path, name):
    code, line = check(tmp_path, COMPILES[name])
    assert code == 0, line
    assert line.endswith(": compiles")


@pytest.mark.parametrize("name", sorted(REFUSED))
def test_refused(tmp_path, name):
    code, line = check(tmp_path, REFUSED[name])
    assert code == 1, line
    assert not line.endswith(": compiles")


def test_too_long(tmp_path):
    code, line = check(tmp_path, "(global long a 0)\n" + " " * 32768)
    assert code == 1
    assert "longer than" in line


def test_unreadable(tmp_path):
    run = run_game(tmp_path, tmp_path / "missing.hsc")
    assert run.returncode != 0
    assert "cannot be read" in run.stderr


def test_writes_no_settings(tmp_path):
    check(tmp_path, COMPILES["lifecycle"])
    assert not (tmp_path / "config.toml").exists()
