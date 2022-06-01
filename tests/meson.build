if get_option('tests')
  mediaart_test = executable('mediaart-test',
    'mediaarttest.c',
    dependencies: libmediaart_dep,
  )

  test('mediaart', mediaart_test,
       env: 'G_TEST_SRCDIR=' + meson.current_source_dir())
endif
