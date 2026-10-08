# Validation record — 0.2.2

## Tested environment

- Kenshi Steam 1.0.65, Windows x64.
- RE_Kenshi 0.3.5 / KenshiLib 0.5.0.
- Visual C++ 2010 x64 and Windows SDK 7.1 for compilation.

This is not a compatibility claim for Kenshi 1.0.68 or other untested combinations. No translation mod is required.

## Automated checks

The distributed `build.ps1 -Test` passed with assertions enabled:

| Test | Coverage |
| --- | --- |
| RaceSectionsTests | 10 groups: detection, preserved text and values, names with punctuation or whitespace, and pagination. |
| InputEventsTests | 15 groups: configured keys, repeats, modifiers, stale input and focus recovery. All four directional arrows are rejected for each of the three configurable actions. |
| TooltipControlGeometryTests | Slow pointer paths through empty cells, dwelling over another item, returning to the source, bent paths and clock wraparound. |
| PageNavigationTests | 8 groups: different page counts in compared panels, consecutive actions, wrapping and collapse. |

The clock-wraparound test intentionally produces unsigned constant overflow and a VC100 C4307 warning; the test passes. This warning is in test code.

## Observed in-game with the locally tested 0.2.2 DLL

The observations below apply to the local build identified by the SHA256 in the binary audit. No inventory test fixture was included in that build. The public build uses the same unchanged source but has **not been retested in-game**. These observations apply to the local binary, not the public rebuild.

- **Rag Loincloth:** the tooltip remained attached to the original item through a slow, segmented path to its controls, crossing empty cells in different inventory sections and pausing for several seconds.
- Its **1,617 displayed race entries** expanded into **270 pages**. Mouse buttons advanced from page 1 to 2, returned from 2 to 1, and wrapped from 1 to 270.
- The controls retained the same vertical position on the shorter last page. Collapse restored the compact list.
- Moving to **Wheatstraw Sunhat** changed the tooltip and showed **1,077 entries**.
- Leaving the tooltip region closed it. Closing and reopening the inventory restored normal hovering and the collapsed Rag Loincloth list.

These entry counts reflect the loaded data in the test environment; other installations will differ.

## Earlier food and weapon evidence

An **instrumented 0.2.0 build**, using temporary native items in a disposable test session, demonstrated:

- **Raw Meat:** its native `Only for:` list contained three displayed entries, including race groups. Collapse, expansion and page changes worked.
- **Desert Sabre:** four native race-specific damage modifiers were retained and grouped. Collapse, expansion and page changes worked.

These are historical observations, **not in-game retests of food and weapons in 0.2.2**. A food without a native `Only for:` list or a weapon without race-specific modifiers is not expected to display these controls.

## Binary audit

The locally tested DLL is a Windows x64 PE32+ image with the `startPlugin` export and the 0.2.2 banner. Its imports are the expected KenshiLib, MyGUI, Ogre, Visual C++ 2010 and Windows runtime libraries. Diagnostic fixture markers are absent.

Locally tested DLL SHA256:

```text
9a26d7c0e57f5a2b1840e59067269015372fb37f5e151988d18b85529fe26ca7
```

The public build uses the same unchanged sources and neutral build paths. Its binary audit found the expected exports and dependencies, with no test fixture or private build-directory path. It has **not been retested in-game**. Public DLL SHA256:

```text
01d9243af5e08d27188630c87c11a390d28d94f45ff64ab4353da1bd0472886d
```

## Limits

- Physical keyboard input was not verified in the 0.2.2 runtime session. F8/Page Up/Page Down and directional-arrow rejection have code-test coverage. Invalid-configuration fallback was inspected in the main code, not deliberately triggered in-game.
- Character switching, focus loss, modal interfaces and compared tooltips with different totals were not all exercised in that runtime session. Relevant state and pagination paths have code tests and review coverage.
- Some Asian glyphs were missing in the game's font on the last page, also observed in 0.2.1. This visual limitation is not evidence that race names were removed from the data.
- Ambiguous list text is preserved when expanded; exact counts or pagination may be unavailable.
- External UI or translation plugins can alter tooltip formats. Compatibility with every mod combination is not established.
