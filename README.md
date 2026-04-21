# wl-kbd

`wl-kbd` is a Wayland keyboard layout tray indicator with a StatusNotifierItem
(SNI) icon and a small tray menu.

It tracks the active layout from Wayland/xkbcommon, shows either a flag icon or
a rendered two-letter icon, and can launch `wl-kbd-config` from the tray.

## Screenshot

![wl-kbd tray menu](screenshot/wl-kbd.png)

## Features

- tray indicator for Wayland sessions using SNI/dbusmenu
- two icon modes:
  - **FLAG**: themed flag icons from `wl-kbd-assets`
  - **LETTERS**: rendered layout abbreviations such as `US`, `RU`
- light/dark theme for letter icons
- configurable letter icon size
- two-line tooltip:
  - configured layouts
  - detected layout-switch shortcut
- tray menu actions:
  - switch between flag and letter icons
  - switch letter icon theme
  - open `wl-kbd-config`
  - quit
- gettext/i18n support

## How it works

- configured layouts are read from `XKB_DEFAULT_LAYOUT`
- the effective current layout is tracked from Wayland keyboard events
- flag icons are loaded by name (`wl-kbd-layout-XX`)
- letter icons are rendered in memory and exported as ARGB pixmaps
- switching between FLAG and LETTERS restarts the process with `-f` / `-l`
  to avoid host-side tray cache issues

## Tooltip

The tray tooltip is shown as two lines:

```text
Layouts: us,ru
Switch shortcut: Alt+Shift
```

Shortcut detection is best-effort:

- for `labwc`, `XKB_DEFAULT_OPTIONS` is read from `~/.config/labwc/environment`
- otherwise it falls back to compositor process environment or config heuristics
- when nothing reliable is found, it shows `unknown`

## Configuration

Configuration is stored in:

```text
~/.config/wl-kbd/indicator.ini
```

Supported keys:

```ini
[Indicator]
tray_style=FLAG
letter_theme=light
letter_icon_size=24
```

Notes:

- `tray_style` = `FLAG` or `LETTERS`
- `letter_theme` = `light` or `dark`
- `letter_icon_size` is clamped to `16..64`

## Command line

```bash
wl-kbd
wl-kbd -f   # start in FLAG mode
wl-kbd -l   # start in LETTERS mode
```

## Build

```bash
meson setup build --prefix=/usr
meson compile -C build
```

Tests:

```bash
meson test -C build
```

Install:

```bash
DESTDIR=/tmp/pkg meson install -C build
```

## Runtime dependencies

- `libsni-exporter`
- `wl-kbd-assets`
- `wayland`
- `libxkbcommon`
- `cairo`
- `pango`

## Related projects

- [`wl-kbd-config`](https://github.com/sfs-pra/wl-kbd-config) — GUI configuration tool for layouts and switching
- [`wl-kbd-assets`](https://github.com/sfs-pra/wl-kbd-assets) — shared flag icons and keyboard layout catalog
- [`libsni-exporter`](https://github.com/sfs-pra/libsni-exporter) — SNI/dbusmenu export library used by the tray indicator
