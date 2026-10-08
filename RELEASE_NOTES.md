# Race List Collapse 0.2.2

This update keeps race-list controls reachable during slow movement across empty inventory cells and restores the directional arrow keys exclusively to the game.

## Changes

- Preserve the tooltip while moving toward its controls, including across inventory sections.
- Switch to another item's native tooltip when the pointer settles over it.
- Remove Left/Right aliases and reject all four directional arrows in the INI. **F8** and **Page Up / Page Down** remain the defaults; mouse **<** and **>** buttons remain available.
- Retain page controls at a consistent height, including the shorter last page.

## Scope

- **Food:** only the game's existing restricted-food `Only for:` lists.
- **Weapons:** existing race-specific damage modifiers. Generic modifiers against humans, animals and robots stay separate.
- **Clothing and armour:** a race-compatibility list derived from native restrictions.

Controls are not expected on every food or weapon. Original loaded names are preserved, and no translation mod is required.

## Install or update

With Kenshi closed, extract the packaged build and copy its `RaceListCollapse` folder into `Kenshi/mods`. Enable it in the launcher and review the INI if updating. No save migration is needed.

Tested with **Kenshi Steam 1.0.65 x64**, **RE_Kenshi 0.3.5** and **KenshiLib 0.5.0**. Compatibility with 1.0.68 or other combinations has not been verified.

## Validation

The locally tested 0.2.2 DLL, SHA256 `9a26d7c0e57f5a2b1840e59067269015372fb37f5e151988d18b85529fe26ca7`, passed mouse checks on **1,617 armour-list entries across 270 pages**, including slow traversal, forward/back navigation, wrapping, collapse, changing items and inventory reopening. All four automated test programs passed.

The public build uses the same unchanged source and has SHA256 `01d9243af5e08d27188630c87c11a390d28d94f45ff64ab4353da1bd0472886d`. It has **not been retested in-game**; the runtime observations above apply to the identified local binary.

Food and weapon lists were observed in an instrumented **0.2.0** test and were **not retested in-game in 0.2.2**. Physical keyboard input was not verified in this runtime session. Some Asian glyphs may be unavailable in the game's font. See [the complete validation record](https://github.com/testeaav02-sudo/RaceListCollapse/blob/main/docs/VALIDATION.md).

Use the repository's [Releases page](https://github.com/testeaav02-sudo/RaceListCollapse/releases/latest) for packaged downloads. Source code is licensed under [GPL-3.0-or-later](https://www.gnu.org/licenses/gpl-3.0.html).
