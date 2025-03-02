project('libmediaart', 'c',
        version: '1.9.7',
        meson_version: '>= 0.56.2',
        default_options: [ 'warning_level=1' ])

gnome = import('gnome')
pkgconfig = import('pkgconfig')

cc = meson.get_compiler('c')

# This is the X.Y used in -lliblibmediaart-FOO-X.Y
libmediaart_api_version = '2.0'
libmediaart_soversion = 0
libmediaart_interface_age = 0

version_array = meson.project_version().split('.')
current = 100 * version_array[1].to_int() + version_array[2].to_int()
libmediaart_ltversion = '@0@.@1@.@2@'.format(libmediaart_soversion, current, libmediaart_interface_age)

glib_required = '2.38.0'

gdk_pixbuf = dependency('gdk-pixbuf-2.0', version: '> 2.12.0', required: false)
glib = dependency('glib-2.0', version: '> ' + glib_required)
gio = dependency('gio-2.0', version: '> ' + glib_required)
gio_unix = dependency('gio-unix-2.0', version: '> ' + glib_required)
gobject = dependency('gobject-2.0', version: '> ' + glib_required)
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
elif get_option('image_library') == 'gdk-pixbuf'
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
  error('No usable image processing backends were found.')
endif

conf = configuration_data()

conf.set('HAVE_GDKPIXBUF', (image_library_name == 'gdk-pixbuf-2.0'),
         description: 'Define if GdkPixbuf is available')
conf.set('HAVE_QT5', (image_library_name == 'Qt5Gui'),
         description: 'Define Qt5 is available')
conf.set('LIBMEDIAART_VERSION', meson.project_version(),
         description: 'Libmediaart version')

visibility_cflags = []
libmediaart_cflags = [
  '-DLIBMEDIAART_COMPILATION'
]

# Symbol visibility.
if get_option('default_library') != 'static'
  if host_machine.system() == 'windows'
    conf.set('DLL_EXPORT', true,
             description: 'Define if DLL_EXPORT is available')
    conf.set('_LIBMEDIAART_EXTERN', '__declspec(dllexport) extern',
             description: 'Define for publicly visible symbols')
    if cc.get_id() != 'msvc'
      visibility_cflags = ['-fvisibility=hidden']
    endif
  else
    conf.set('_LIBMEDIAART_EXTERN', '__attribute__((visibility("default"))) extern',
             description: 'Define for publicly visible symbols')
    visibility_cflags = ['-fvisibility=hidden']
  endif
endif

configure_file(output: 'config.h',
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

summary('prefix', get_option('prefix'), section: 'Directories')
summary('includedir', get_option('includedir'), section: 'Directories')
summary('libdir', get_option('libdir'), section: 'Directories')

summary('Image processing library', image_library_name, section: 'Build')
summary('Documentation', get_option('gtk_doc'), section: 'Build', bool_yn: true)
