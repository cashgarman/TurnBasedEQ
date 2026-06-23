#!/usr/bin/env python3
"""Build normalized 128x256 entity sprite atlases from open-source CC0 assets."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageEnhance

FRAME = 32
ATLAS_W = FRAME * 4
ATLAS_H = FRAME * 8
FACING = ("south", "east", "north", "west")


def blank_frame() -> Image.Image:
    return Image.new("RGBA", (FRAME, FRAME), (0, 0, 0, 0))


def crop_frame(sheet: Image.Image, col: int, row: int) -> Image.Image:
    return sheet.crop((col * FRAME, row * FRAME, (col + 1) * FRAME, (row + 1) * FRAME))


def mirror_h(frame: Image.Image) -> Image.Image:
    return frame.transpose(Image.Transpose.FLIP_LEFT_RIGHT)


def expand_frames(frames: list[Image.Image], count: int = 4) -> list[Image.Image]:
    if not frames:
        return [blank_frame()] * count
    if len(frames) >= count:
        return frames[:count]
    out = list(frames)
    while len(out) < count:
        out.append(frames[len(out) % len(frames)])
    return out


def tint_image(image: Image.Image, hue_shift: float = 0.0, sat_mult: float = 1.0, light_shift: float = 0.0) -> Image.Image:
    if hue_shift == 0.0 and sat_mult == 1.0 and light_shift == 0.0:
        return image.copy()

    result = image.copy()
    pixels = result.load()
    for y in range(result.height):
        for x in range(result.width):
            r, g, b, a = pixels[x, y]
            if a < 8:
                continue
            rf, gf, bf = r / 255.0, g / 255.0, b / 255.0
            mx, mn = max(rf, gf, bf), min(rf, gf, bf)
            l = (mx + mn) * 0.5
            if mx == mn:
                h = 0.0
                s = 0.0
            else:
                d = mx - mn
                s = d / (2.0 - mx - mn) if l > 0.5 else d / (mx + mn)
                if mx == rf:
                    h = (gf - bf) / d + (6.0 if gf < bf else 0.0)
                elif mx == gf:
                    h = (bf - rf) / d + 2.0
                else:
                    h = (rf - gf) / d + 4.0
                h /= 6.0

            h = (h + hue_shift / 360.0) % 1.0
            s = max(0.0, min(1.0, s * sat_mult))
            l = max(0.05, min(0.95, l + light_shift))

            if s == 0.0:
                gray = int(l * 255.0)
                pixels[x, y] = (gray, gray, gray, a)
                continue

            def hue_to_rgb(p: float, q: float, t: float) -> float:
                if t < 0.0:
                    t += 1.0
                if t > 1.0:
                    t -= 1.0
                if t < 1.0 / 6.0:
                    return p + (q - p) * 6.0 * t
                if t < 1.0 / 2.0:
                    return q
                if t < 2.0 / 3.0:
                    return p + (q - p) * (2.0 / 3.0 - t) * 6.0
                return p

            q = l * (1.0 + s) if l < 0.5 else l + s - l * s
            p = 2.0 * l - q
            nr = int(hue_to_rgb(p, q, h + 1.0 / 3.0) * 255.0)
            ng = int(hue_to_rgb(p, q, h) * 255.0)
            nb = int(hue_to_rgb(p, q, h - 1.0 / 3.0) * 255.0)
            pixels[x, y] = (nr, ng, nb, a)
    return result


def colorize(image: Image.Image, hex_color: str, mix: float) -> Image.Image:
    color = hex_color.lstrip("#")
    tr = int(color[0:2], 16)
    tg = int(color[2:4], 16)
    tb = int(color[4:6], 16)
    result = image.copy()
    pixels = result.load()
    for y in range(result.height):
        for x in range(result.width):
            r, g, b, a = pixels[x, y]
            if a < 8:
                continue
            t = mix
            pixels[x, y] = (
                int(r + (tr - r) * t),
                int(g + (tg - g) * t),
                int(b + (tb - b) * t),
                a,
            )
    return result


def build_atlas(idle_by_facing: dict[str, list[Image.Image]], walk_by_facing: dict[str, list[Image.Image]]) -> Image.Image:
    atlas = Image.new("RGBA", (ATLAS_W, ATLAS_H), (0, 0, 0, 0))
    for clip_offset, clip in enumerate(("idle", "walk")):
        source = idle_by_facing if clip == "idle" else walk_by_facing
        for facing_index, facing in enumerate(FACING):
            row = clip_offset * 4 + facing_index
            frames = expand_frames(source.get(facing, [blank_frame()]))
            for col, frame in enumerate(frames):
                atlas.paste(frame, (col * FRAME, row * FRAME), frame)
    return atlas


def load_rpg_hero(source_dir: Path) -> tuple[dict[str, list[Image.Image]], dict[str, list[Image.Image]]]:
    sheet = Image.open(source_dir / "rpg-character.png").convert("RGBA")
    # CC art order: down, left, right, up
    row_map = {
        "south": 0,
        "west": 1,
        "east": 2,
        "north": 3,
    }
    idle: dict[str, list[Image.Image]] = {}
    walk: dict[str, list[Image.Image]] = {}
    for facing, row in row_map.items():
        walk[row_key := facing] = [crop_frame(sheet, col, row) for col in range(4)]
        idle[row_key] = [crop_frame(sheet, col, row + 4) for col in range(4)]
    return idle, walk


def load_farmer(source_dir: Path, color_hex: str | None = None) -> tuple[dict[str, list[Image.Image]], dict[str, list[Image.Image]]]:
    stand = Image.open(source_dir / "stand.png").convert("RGBA")
    direction_files = {
        "north": "npc0.png",
        "east": "npc1.png",
        "south": "npc2.png",
        "west": "npc3.png",
    }
    idle: dict[str, list[Image.Image]] = {}
    walk: dict[str, list[Image.Image]] = {}
    for facing_index, facing in enumerate(FACING):
        stand_frame = crop_frame(stand, facing_index, 0)
        idle[facing] = expand_frames([stand_frame])
        sheet = Image.open(source_dir / direction_files[facing]).convert("RGBA")
        walk[facing] = [crop_frame(sheet, col, 0) for col in range(4)]
        if color_hex:
            idle[facing] = [colorize(frame, color_hex, 0.35) for frame in idle[facing]]
            walk[facing] = [colorize(frame, color_hex, 0.35) for frame in walk[facing]]
    return idle, walk


def load_rat(source_path: Path) -> tuple[dict[str, list[Image.Image]], dict[str, list[Image.Image]]]:
    sheet = Image.open(source_path).convert("RGBA")
    direction_cols = {
        "south": 1,
        "east": 5,
        "north": 9,
        "west": 13,
    }
    idle: dict[str, list[Image.Image]] = {}
    walk: dict[str, list[Image.Image]] = {}
    for facing, base_col in direction_cols.items():
        idle[facing] = expand_frames([crop_frame(sheet, base_col, 3)])
        frame_a = crop_frame(sheet, base_col, 1)
        frame_b = crop_frame(sheet, base_col + 1, 1)
        frame_c = crop_frame(sheet, base_col, 2)
        frame_d = crop_frame(sheet, base_col + 1, 2)
        walk[facing] = expand_frames([frame_a, frame_b, frame_c, frame_d])
    return idle, walk


def load_boar(source_path: Path) -> tuple[dict[str, list[Image.Image]], dict[str, list[Image.Image]]]:
    sheet = Image.open(source_path).convert("RGBA")
    idle: dict[str, list[Image.Image]] = {}
    walk: dict[str, list[Image.Image]] = {}
    south_walk = [crop_frame(sheet, col, 1) for col in range(4)]
    south_idle = expand_frames([crop_frame(sheet, 0, 0)])
    for facing in FACING:
        walk[facing] = list(south_walk)
        idle[facing] = list(south_idle)
        if facing == "west":
            walk[facing] = [mirror_h(frame) for frame in walk[facing]]
            idle[facing] = [mirror_h(frame) for frame in idle[facing]]
        elif facing == "north":
            walk[facing] = [crop_frame(sheet, col, 4) for col in range(4)]
            idle[facing] = expand_frames([crop_frame(sheet, 0, 4)])
        elif facing == "east":
            walk[facing] = [crop_frame(sheet, col, 5) for col in range(4)]
            idle[facing] = expand_frames([crop_frame(sheet, 0, 5)])
    return idle, walk


def load_goblin(source_path: Path, row: int = 0) -> tuple[dict[str, list[Image.Image]], dict[str, list[Image.Image]]]:
    sheet = Image.open(source_path).convert("RGBA").convert("RGBA")
    if sheet.mode == "P":
        sheet = sheet.convert("RGBA")
    idle_frame = crop_frame(sheet, 0, row)
    walk_frames = [crop_frame(sheet, col, row) for col in range(1, 5)]
    idle: dict[str, list[Image.Image]] = {}
    walk: dict[str, list[Image.Image]] = {}
    for facing in FACING:
        idle[facing] = expand_frames([idle_frame])
        frames = list(walk_frames)
        if facing == "west":
            frames = [mirror_h(frame) for frame in frames]
        walk[facing] = frames
    return idle, walk


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=Path("data/sprites"))
    parser.add_argument("--output", type=Path, default=Path("data/sprites/entities"))
    args = parser.parse_args()

    source = args.source
    output = args.output
    output.mkdir(parents=True, exist_ok=True)

    hero_idle, hero_walk = load_rpg_hero(source / "extracted/rpg_hero")
    build_atlas(hero_idle, hero_walk).save(output / "player.png")

    build_atlas(*load_farmer(source / "extracted/farmer", "#c9a227")).save(output / "npc_merchant.png")
    build_atlas(*load_farmer(source / "extracted/farmer", "#6a8fc7")).save(output / "npc_lore.png")
    build_atlas(*load_farmer(source / "extracted/farmer", "#7ab86a")).save(output / "npc_quest.png")

    build_atlas(*load_rat(source / "rat-move.png")).save(output / "mob_rat.png")
    build_atlas(*load_boar(source / "boar.png")).save(output / "mob_boar.png")
    build_atlas(*load_goblin(source / "goblin.png", row=0)).save(output / "mob_goblin.png")
    build_atlas(*load_goblin(source / "goblin.png", row=2)).save(output / "mob_goblin_chief.png")

    print(f"Wrote entity atlases to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
