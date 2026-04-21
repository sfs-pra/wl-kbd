glib_dep = dependency('glib-2.0')

test_shortcut_hint = executable('test-shortcut-hint',
  [
    'test_shortcut_hint.c',
    '../src/shortcut_hint.c',
  ],
  dependencies: [glib_dep],
  include_directories: include_directories('../src'),
)

test('shortcut-hint', test_shortcut_hint)
