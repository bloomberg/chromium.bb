if not get_option('xwayland')
	subdir_done()
endif

srcs_xwayland = [
	'launcher.c',
	'window-manager.c',
	'selection.c',
	'dnd.c',
	'hash.c',
]
deps_xwayland = [
	dependency('xcb'),
	dependency('xcb-composite'),
	dependency('xcb-shape'),
	dependency('xcb-xfixes'),
	dependency('xcursor'),
	dependency('cairo-xcb'),
	dep_libweston,
]
plugin_xwayland = shared_library(
	'xwayland',
	srcs_xwayland,
	link_with: lib_cairo_shared,
	include_directories: include_directories('..', '../shared'),
	dependencies: deps_xwayland,
	name_prefix: '',
	install: true,
	install_dir: dir_module_libweston
)
env_modmap += 'xwayland.so=@0@;'.format(plugin_xwayland.full_path())

install_headers('xwayland-api.h', subdir: dir_include_libweston)
