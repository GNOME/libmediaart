libmediaart_headers = [
  'cache.h',
  'extract.h',
  'extractgeneric.h',
  'storage.h'
]

libmediaart_sources = [
  'cache.c',
  'extract.c',
  'storage.c'
]

if image_library_name == 'gdk-pixbuf'
  libmediaart_sources += 'extractpixbuf.c'
elif image_library_name == 'qt4' or image_library_name == 'qt5'
  libmediaart_sources += 'extractqt.c'
else
  libmediaart_sources += 'extractdummy.c'
endif

marshal = gnome.genmarshal('marshal',
  sources: 'marshal.list',
  prefix: 'media_art_marshal')

libmediaart_dependencies = [gio, gio_unix, glib, gobject, image_library]

libmediaart = library(
  'libmediaart-' + libmediaart_api_version,
  libmediaart_sources, marshal[0], marshal[1],
  c_args: ['-DLIBMEDIAART_COMPILATION'],
  dependencies: libmediaart_dependencies,
  include_directories: configinc,
  install: true,
)

libmediaart_dep = declare_dependency(
  link_with: libmediaart,
  dependencies: libmediaart_dependencies,
  include_directories: configinc
)

libmediaart_gir = gnome.generate_gir(libmediaart,
  sources: libmediaart_sources + libmediaart_headers,
  nsversion: libmediaart_api_version,
  namespace: 'MediaArt',
  identifier_prefix: 'MediaArt',
  symbol_prefix: 'media_art',
  includes: ['Gio-2.0', 'GObject-2.0'],
  extra_args: [
    '--c-include=libmediaart/mediaart.h',
    '-D', 'LIBMEDIAART_COMPILATION',
  ],
  install: true
)

libmediaart_vapi = gnome.generate_vapi('libmediaart-' + libmediaart_api_version,
  sources: libmediaart_gir[0],
  packages: 'gio-2.0')
