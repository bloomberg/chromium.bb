man_pages = [
	{
		'section': '3',
		'xml': 'wl_display_connect.xml',
		'name': 'wl_display_connect',
		'alias': 'wl_display_connect_to_fd',
	}
]

xsltproc_opts = [
	'--nonet',
	'--stringparam', 'man.authors.section.enabled', '0',
	'--stringparam', 'man.copyright.section.enabled', '0',
	'--stringparam', 'funcsynopsis.style', 'ansi',
	'--stringparam', 'man.output.quietly', '1',
]

foreach page: man_pages
	section_number = page['section']
	xml_input = page['xml']
	name = page['name']
	alias = page.get('alias', '')

	man_output = name + '.' + section_number
	if alias != ''
		alias_output = alias + '.' + section_number
	else
		alias_output = []
	endif

	man_page = custom_target(
		name + '-man',
		command: [
			xsltproc,
			xsltproc_opts,
			'-o', '@OUTPUT0@',
			manpage_xsl,
			'@INPUT@',
		],
		input: xml_input,
		output: [ man_output, alias_output ],
		install: true,
		install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'man' + section_number),
		build_by_default: true,
	)
endforeach
