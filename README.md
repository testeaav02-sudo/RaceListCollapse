# Race List Collapse

A Kenshi + RE_Kenshi plugin that collapses long race lists in inventory tooltips and expands them into pages.

[Português brasileiro](README.pt-BR.md) · [Releases and downloads](https://github.com/testeaav02-sudo/RaceListCollapse/releases/latest) · [Validation](docs/VALIDATION.md)

## Which items show controls?

| Item | Supported list |
| --- | --- |
| Restricted food | The game's existing **Only for:** list. Ordinary food without this native list does not gain one. |
| Weapons | Existing race-specific bonuses and penalties from **race damage**. Generic modifiers against humans, animals and robots remain visible separately. |
| Clothing and armour | A **Wearable by:** list derived from the game's race restrictions and equipment-slot rules. |

**The controls do not appear on every food or weapon.** The plugin does not generate a universal edible-by list or a list of every race for every weapon. Armour compatibility describes race eligibility; missing limbs, occupied slots and other character conditions can still prevent equipping an item.

Names come from loaded race and race-group records, including mods. The plugin does not translate those names or require a translation mod. Native translated headings are recognized where available. It changes tooltip presentation, without changing item stats, race rules or saves.

## Requirements

Tested configuration:

- Kenshi **Steam 1.0.65**, Windows x64.
- [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) **0.3.5** with **KenshiLib 0.5.0**.

Other combinations, including Kenshi 1.0.68, have not been verified. Kenshi, RE_Kenshi and their runtime libraries are not included.

## Install

1. Close Kenshi and extract a packaged build from [Releases](https://github.com/testeaav02-sudo/RaceListCollapse/releases/latest).
2. Copy its `RaceListCollapse` folder into Kenshi's `mods` folder. It must contain `RaceListCollapse.mod`, `RaceListCollapse.dll`, `RaceListCollapse.ini` and `RE_Kenshi.json`.
3. Enable **RaceListCollapse** in the launcher's Mods tab, then launch your existing RE_Kenshi installation.

For an update, replace the files with the game closed and review your INI settings. GitHub's automatic source-code archive is for development, not installation.

## Use

- Hover a supported item, move to **Expand races** and click it.
- Use the on-screen **<** and **>** buttons to change pages; they appear only when more than one page is needed. **Collapse** closes the list.
- **F8** toggles the list. **Page Up / Page Down** change pages.
- The four directional arrow keys are reserved for the game and cannot be assigned to this plugin.
- Use shortcuts without Ctrl, Shift or Alt while the game has focus and a supported tooltip is visible.

The default is six entries per page, shared across the item's supported sections. Expansion is shared between tooltips during the session. Counts represent displayed entries, which may include race groups.

The tooltip remains available while crossing empty inventory cells toward its controls. Pausing over a different item lets its description take over. The page buttons keep their height when the last page has fewer entries.

## Configure or remove

Edit `RaceListCollapse.ini` with the game closed:

```ini
[General]
Enabled=1
PageSize=6
DebugLogging=0

[Keys]
Toggle=119
PreviousPage=33
NextPage=34
```

`PageSize` accepts 1–12. `Enabled=0` disables the plugin. `DebugLogging=1` writes diagnostic tooltip details to the RE_Kenshi log. Keys use decimal Windows virtual-key codes. Directional arrows (37–40) are rejected; invalid mappings restore F8/Page Up/Page Down. Restart after changes.

To uninstall, disable the mod in the launcher and remove only `mods/RaceListCollapse` with the game closed. No save migration is required.

## Validation and limitations

A local build of version 0.2.2 was checked in-game with an armour list of **1,617 entries across 270 pages**, including slow movement to the controls, paging, collapse, item changes and inventory reopening. The public binary was rebuilt from the same unchanged source and was **not retested in-game**. Food and weapon lists were observed in an instrumented 0.2.0 test; they were **not retested in-game in 0.2.2**.

Some Asian characters may not render in the game's font. Ambiguous list formats remain fully available when expanded, but may not support pagination. Physical keyboard input and every external mod combination are not covered by the latest runtime checks. See [the validation record](docs/VALIDATION.md) for precise coverage.

## Build

Use **Visual C++ 2010 x64 (VC100)**, **Windows SDK 7.1** and the official [KenshiLib example dependencies](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps), fetched with Git LFS. A newer compiler is not an ABI-compatible substitute. Extract the dependency bundle's Boost archive before building.

```powershell
.\build.ps1 -DepsRoot 'D:\SDK\KenshiLib_Examples_deps' -VcRoot 'D:\SDK\VC100\VC' -SdkRoot 'D:\SDK\Windows\v7.1'
```

The result is `build/RaceListCollapse.dll`. Add `-Test` to build and run the four test programs instead. Portable toolchains with separate headers can use `-VcIncludeRoot`; `-OutputDirectory` changes the output location. The script enables `/MD`, `/GL` and `/LTCG` and does not install into the game.

## License

Plugin source: [GPL-3.0-or-later](https://www.gnu.org/licenses/gpl-3.0.html). Kenshi and third-party dependencies retain their own licenses and are obtained separately. Built using [KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib) and its [plugin examples](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples).
