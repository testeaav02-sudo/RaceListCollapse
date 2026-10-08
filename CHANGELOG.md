# Changelog

## 0.2.2

- Keep a supported tooltip available while moving slowly through empty inventory cells toward its controls, including paths across inventory sections.
- Let dwelling over another item return control to native tooltip selection.
- Remove automatic Left/Right keyboard aliases. Reject all four directional arrow keys even in manually edited configuration; retain F8 and Page Up/Page Down defaults.
- Add pointer-transition tests and expand keyboard tests to cover prohibited mappings.
- Confirm mouse navigation on an armour list with 1,617 entries and 270 pages, item changes, collapse and inventory reopening using the local test build; the public rebuild uses unchanged source and was not retested in-game.

## 0.2.1

- Navigate using the displayed panel's own page count, avoiding apparently unresponsive buttons when a comparison has more pages.
- Keep page controls at a stable height on shorter pages.
- Allow controls on a visible tooltip whose source widget is temporarily disabled.
- Add Left/Right aliases, subsequently removed in 0.2.2 to avoid interfering with game controls.

## 0.2.0

- Add clickable expand/collapse and paging controls.
- Add armour compatibility lists using native race and equipment restrictions.
- Preserve the captured item and comparison text when changing pages.
- Handle race names containing boundary whitespace and punctuation.
- Verify Raw Meat and Desert Sabre lists with an instrumented native test build.

Food support uses the existing native `Only for:` list. Weapon support groups existing race-specific modifiers and leaves generic human, animal and robot modifiers separate. These versions do not add universal lists to every food or weapon. See [validation coverage](docs/VALIDATION.md).
