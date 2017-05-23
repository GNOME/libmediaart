project('libmediaart', 'c', 'cpp', version: '1.9.1')

gnome = import('gnome')

gnome = import('gnome')
pkgconfig = import('pkgconfig')

cc = meson.get_compiler('c')

# This is the X.Y used in -lliblibmediaart-FOO-X.Y
libmediaart_api_version = '2.0'

glib_required = '2.38.0'

gdk_pixbuf = dependency('gdk-pixbuf-2.0', version: '> 2.12.0', required: false)
glib = dependency('glib-2.0', version: '> ' + glib_required)
gio = dependency('gio-2.0', version: '> ' + glib_required)
gio_unix = dependency('gio-unix-2.0', version: '> ' + glib_required)
gobject = dependency('gobject-2.0', version: '> ' + glib_required)
qt4 = dependency('qt4', version: '> 4.7.1', modules: 'Gui', required: false)
qt5 = dependency('qt5', version: '> 5.0.0', modules: 'Gui', required: false)

##################################################################
# Choose between backends (GdkPixbuf/Qt/etc)
##################################################################

image_library_name = ''

if gdk_pixbuf.found()
  if get_option('image_library') == 'auto' or get_option('image_library') == 'gdk-pixbuf'
    image_library = gdk_pixbuf
    image_library_name = 'gdk-pixbuf-2.0'
  endif
elif get_option('image_library') == 'icu'
  error('gdk-pixbuf backend explicitly requested, but gdk-pixbuf library was not found')
endif

if image_library_name == ''
  if qt5.found()
    if get_option('image_library') == 'auto' or get_option('image_library') == 'qt5'
      image_library = qt5
      image_library_name = 'Qt5Gui'
    endif
  elif get_option('image_library') == 'qt5'
    error('qt5 explicitly requested, but not found')
  endif
endif

if image_library_name == ''
  if qt4.found()
    if get_option('image_library') == 'auto' or get_option('image_library') == 'qt4'
      image_library = qt4
      image_library_name = 'QtGui'
    endif
  elif get_option('image_library') == 'qt4'
    error('qt4 explicitly requested, but not found')
  endif
endif

if image_library_name == ''
  error('No usable image processing backends were found.')
endif

conf = configuration_data()

conf.set('HAVE_GDKPIXBUF', (image_library_name == 'gdk-pixbuf-2.0'))
conf.set('HAVE_QT', (image_library_name == 'QtGui' or image_library_name == 'Qt5Gui'))
conf.set('HAVE_QT4', (image_library_name == 'QtGui'))
conf.set('HAVE_QT5', (image_library_name == 'Qt5Gui'))
conf.set('LIBMEDIAART_VERSION', meson.project_version())

libmediaart_cflags = [
  '-DLIBMEDIAART_COMPILATION'
]

# Symbol visibility.
if get_option('default_library') != 'static'
  if host_machine.system() == 'windows'
    conf.set('DLL_EXPORT', true)
    conf.set('_LIBMEDIAART_EXTERN', '__declspec(dllexport) extern')
    if cc.get_id() != 'msvc'
      libmediaart_cflags += ['-fvisibility=hidden']
    endif
  else
    conf.set('_LIBMEDIAART_EXTERN', '__attribute__((visibility("default"))) extern')
    libmediaart_cflags += ['-fvisibility=hidden']
  endif
endif

configure_file(input: 'config.h.meson.in',
               output: 'config.h',
               configuration: conf)

root_inc = include_directories('.')

subdir('libmediaart')
subdir('docs')
subdir('tests')

pkgconfig.generate(
  libraries: libmediaart,
  name: 'libmediaart- ' + libmediaart_api_version,
  version: meson.project_version(),
  description: 'libmediaart - Media art extraction and cache management library',
  filebase: 'libmediaart-' + libmediaart_api_version,
  subdirs: 'libmediaart-' + libmediaart_api_version,
  requires: 'glib-2.0',
  requires_private: image_library_name,
  libraries_private: ['-lz', '-lm'])

summary = [
  '\nBuild Configuration:',
  '    Prefix:                                 ' + get_option('prefix'),
  '    Source code location:                   ' + meson.source_root(),
  '    Compiler:                               ' + cc.get_id(),
  '    Image processing library:               ' + image_library_name,
]

message('\n'.join(summary))
