project('wl-kbd', 'c',
  version: '0.1.0',
  license: 'MIT',
  default_options: ['warning_level=2', 'c_std=c11']
)

i18n = import('i18n')

gio_dep        = dependency('gio-2.0')
cairo_dep      = dependency('cairo')
pangocairo_dep = dependency('pangocairo')
wl_dep         = dependency('wayland-client')
xkb_dep        = dependency('xkbcommon')
sni_dep        = dependency('sni-exporter')

desktop_file = configure_file(
  input: 'wl-kbd.desktop.in',
  output: 'wl-kbd.desktop',
  copy: true,
)

install_data(
  desktop_file,
  install_dir: join_paths(get_option('datadir'), 'applications')
)
install_data(
  desktop_file,
  install_dir: '/etc/xdg/autostart'
)
install_data(
  'doc/man1/wl-kbd.1',
  install_dir: join_paths(get_option('mandir'), 'man1')
)

subdir('po')
subdir('src')
subdir('tests')
