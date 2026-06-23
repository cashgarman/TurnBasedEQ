# Entity Sprite Attribution

TurnBasedEQ uses CC0 (public domain) sprite assets from OpenGameArt.org and the
Liberated Pixel Cup. Normalized atlases in `entities/` are derived from the
source files below.

| Atlas | Source | Author | License |
|-------|--------|--------|---------|
| `player.png` | [RPG Hero (walking cycle)](https://opengameart.org/content/rpg-hero-walking-cycle) | CC Art / OpenGameArt | CC0 |
| `npc_*.png` | [PagesOfAdventure farmer sprite sheet](https://opengameart.org/content/pagesofadventure-farmer-sprite-sheet) | PagesOfAdventure team | CC0 |
| `mob_rat.png` | [Top down rat, animated](https://lpc.opengameart.org/content/top-down-rat-animated) | Warlock's Gauntlet artists | CC-BY-SA 3.0 / GPL 3.0 |
| `mob_boar.png` | [Boar 32x32](https://opengameart.org/content/boar-32x32) | OpenGameArt contributor | CC0 |
| `mob_goblin*.png` | [Goblin monster](https://opengameart.org/content/goblin-monster) | Imogia (remix of MoikMellah) | CC0 |

Rebuild atlases with:

```bash
python tools/build_entity_sprites.py
```

Source downloads used by the build script live under `data/sprites/` (archives and
extracted folders). Remove `extracted/` and loose downloads before release if
you only want to ship the normalized `entities/` atlases.
