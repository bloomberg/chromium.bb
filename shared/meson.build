srcs_libshared = [
	'config-parser.c',
	'option-parser.c',
	'file-util.c',
	'os-compatibility.c',
	'xalloc.c',
]
deps_libshared = dep_wayland_client

lib_libshared = static_library(
	'shared',
	srcs_libshared,
	include_directories: include_directories('..'),
	dependencies: deps_libshared,
	install: false
)
dep_libshared = declare_dependency(
	link_with: lib_libshared,
	dependencies: deps_libshared
)

srcs_cairo_shared = [
	'image-loader.c',
	'cairo-util.c',
	'frame.c',
]

deps_cairo_shared = [
	dep_libshared,
	dependency('cairo'),
	dependency('libpng'),
	dep_pixman,
	dep_libm,
]

dep_pango = dependency('pango', required: false)
dep_pangocairo = dependency('pangocairo', required: false)
dep_glib = dependency('glib-2.0', version: '>= 2.36', required: false)

if dep_pango.found() and dep_pangocairo.found() and dep_glib.found()
	deps_cairo_shared += [ dep_pango, dep_pangocairo, dep_glib ]
	config_h.set('HAVE_PANGO', '1')
endif

if get_option('image-jpeg')
	deps_cairo_shared += cc.find_library('jpeg')
	config_h.set('HAVE_JPEG', '1')
endif

if get_option('image-webp')
	deps_cairo_shared += dependency('libwebp')
	config_h.set('HAVE_WEBP', '1')
endif

lib_cairo_shared = static_library(
	'cairo-shared',
	srcs_cairo_shared,
	include_directories: include_directories('..'),
	dependencies: deps_cairo_shared,
	install: false
)
dep_lib_cairo_shared = declare_dependency(
	link_with: lib_cairo_shared,
	dependencies: deps_cairo_shared
)
