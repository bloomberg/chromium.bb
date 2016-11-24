if not get_option('wcap-decode')
	subdir_done()
endif

srcs_wcap = [
	'main.c',
	'wcap-decode.c',
]

deps_wcap = [
	dep_libm,
	dependency('cairo'),
]

executable(
	'wcap-decode',
	srcs_wcap,
	include_directories: include_directories('..'),
	dependencies: deps_wcap,
	install: true
)
