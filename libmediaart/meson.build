libmediaart_public_headers = [
  'mediaart-macros.h',
  'cache.h',
  'extract.h',
  'extractgeneric.h',
  'mediaart.h',
]

libmediaart_sources = [
  'cache.c',
  'extract.c',
  'storage.c'
]

if image_library_name == 'gdk-pixbuf-2.0'
  libmediaart_sources += 'extractpixbuf.c'
elif image_library_name == 'QtGui' or image_library_name == 'Qt5Gui'
  libmediaart_sources += 'extractqt.cpp'
else
  libmediaart_sources += 'extractdummy.c'
endif

marshal = gnome.genmarshal('marshal',
  sources: 'marshal.list',
  prefix: 'media_art_marshal')

libmediaart_dependencies = [glib, gio_unix, gobject, image_library]

libmediaart = shared_library(
  'mediaart-' + libmediaart_api_version,
  libmediaart_sources, marshal[0], marshal[1],
  version: libmediaart_ltversion,
  soversion: libmediaart_soversion,
  dependencies: libmediaart_dependencies,
  c_args: libmediaart_cflags + visibility_cflags,
  include_directories: root_inc,
  install: true,
)

libmediaart_gir_and_typelib = gnome.generate_gir(libmediaart,
  sources: libmediaart_sources + libmediaart_public_headers,
  nsversion: libmediaart_api_version,
  namespace: 'MediaArt',
  identifier_prefix: 'MediaArt',
  symbol_prefix: 'media_art',
  includes: ['Gio-2.0', 'GObject-2.0'],
  extra_args: libmediaart_cflags + [
    '--c-include=libmediaart/mediaart.h',
  ],
  install: true
)

libmediaart_vapi = gnome.generate_vapi('libmediaart-' + libmediaart_api_version,
  sources: libmediaart_gir_and_typelib[0],
  packages: 'gio-2.0',
  install: true)

libmediaart_dep = declare_dependency(
  link_with: libmediaart,
  dependencies: libmediaart_dependencies,
  include_directories: root_inc,
)

install_headers(libmediaart_public_headers,
  subdir: 'libmediaart-@0@/libmediaart'.format(libmediaart_api_version))
