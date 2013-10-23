{
    'variables': {
        'scripts_for_in_files': [
            # jinja2/__init__.py contains version string, so sufficient as
            # dependency for whole jinja2 package
            '<(DEPTH)/third_party/jinja2/__init__.py',
            '<(DEPTH)/third_party/markupsafe/__init__.py',  # jinja2 dep
            'hasher.py',
            'in_file.py',
            'in_generator.py',
            'license.py',
            'name_macros.py',
            'name_utilities.py',
            'template_expander.py',
            'templates/macros.tmpl',
        ],
        'make_names_files': [
            '<@(scripts_for_in_files)',
            'make_names.py',
            'templates/MakeNames.cpp.tmpl',
            'templates/MakeNames.h.tmpl',
        ],
        'make_qualified_names_files': [
            '<@(scripts_for_in_files)',
            'make_qualified_names.py',
            'templates/MakeQualifiedNames.cpp.tmpl',
            'templates/MakeQualifiedNames.h.tmpl',
        ],
        'make_element_factory_files': [
            '<@(make_qualified_names_files)',
            'make_element_factory.py',
            'templates/ElementFactory.cpp.tmpl',
            'templates/ElementFactory.h.tmpl',
            'templates/ElementWrapperFactory.cpp.tmpl',
            'templates/ElementWrapperFactory.h.tmpl',
        ],
    },
}
