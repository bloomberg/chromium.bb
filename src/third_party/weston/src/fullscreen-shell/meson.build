if get_option('shell-fullscreen')
	srcs_shell_fullscreen = [
		'fullscreen-shell.c',
		'../shared/matrix.c',
		fullscreen_shell_unstable_v1_server_protocol_h,
		fullscreen_shell_unstable_v1_protocol_c,
	]
	shared_library(
		'fullscreen-shell',
		srcs_shell_fullscreen,
		include_directories: include_directories('..', '../shared'),
		dependencies: dep_libweston,
		name_prefix: '',
		install: true,
		install_dir: dir_module_weston
	)
endif
