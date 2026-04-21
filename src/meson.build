executable('wl-kbd',
  [
    'main.c',
    'config.c',
    'icon.c',
    'layout_code.c',
    'wl_keyboard.c',
    'indicator.c',
    'shortcut_hint.c',
  ],
  dependencies: [gio_dep, cairo_dep, pangocairo_dep, wl_dep, xkb_dep, sni_dep],
  c_args: ['-DGETTEXT_PACKAGE="wl-kbd"'],
  install: true,
)
