# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

"""Generates Blink Web Module bindings.

The Blink Web Module bindings provide a stable, IDL-generated interface for the
Web Modules.

The Web Modules are the high-level services like Autofill,
Autocomplete, Translate, Distiller, Phishing Detector, and others. Web Modules
typically want to introspec the document and rendering infromation to implement
browser features.

The bindings are meant to be as simple and as ephemeral as possible, mostly just
wrapping existing DOM classes. Their primary goal is to avoid leaking the actual
DOM classes to the Web Modules layer.
"""

import os
import posixpath

from code_generator import CodeGeneratorBase, render_template
# TODO(dglazkov): Move TypedefResolver to code_generator.py
from code_generator_v8 import TypedefResolver

MODULE_PYNAME = os.path.splitext(os.path.basename(__file__))[0] + '.py'

WEB_MODULE_IDL_ATTRIBUTE = 'WebModuleAPI'
STRING_INCLUDE_PATH = 'wtf/text/WTFString.h'

def interface_context(idl_interface):
    builder = InterfaceContextBuilder(MODULE_PYNAME, TypeResolver())
    builder.set_class_name(idl_interface.name)
    builder.set_inheritance(idl_interface.parent)

    for idl_attribute in idl_interface.attributes:
        builder.add_attribute(idl_attribute)

    for idl_operation in idl_interface.operations:
        builder.add_operation(idl_operation)

    return builder.build()


class TypeResolver(object):
    """Resolves Web IDL types into corresponding C++ types and include paths
       to the generated and existing files."""

    def includes_from_interface(self, base_type):
        # TODO(dglazkov): Are there any exceptional conditions here?
        return set([base_type])

    def _includes_from_type(self, idl_type):
        if idl_type.is_void:
            return set()
        if idl_type.is_primitive_type:
            return set()
        if idl_type.is_string_type:
            return set([STRING_INCLUDE_PATH])

        # TODO(dglazkov): Handle complex/weird types.
        # TODO(dglazkov): Make these proper paths to generated and non-generated
        # files.
        return set([idl_type.base_type])

    def includes_from_definition(self, idl_definition):
        return self._includes_from_type(idl_definition.idl_type)

    def type_from_definition(self, idl_definition):
        # TODO(dglazkov): The output of this method must be a reasonable C++
        # type that can be used directly in the jinja2 template.
        return idl_definition.idl_type.base_type


class InterfaceContextBuilder(object):
    def __init__(self, code_generator, type_resolver):
        self.result = {'code_generator': code_generator}
        self.type_resolver = type_resolver

    def set_class_name(self, class_name):
        self.result['class_name'] = class_name

    def set_inheritance(self, base_interface):
        if base_interface is None:
            return
        self.result['inherits_expression'] = ' : public %s' % base_interface
        self._ensure_set('cpp_includes').update(
            self.type_resolver.includes_from_interface(base_interface))

    def _ensure_set(self, name):
        return self.result.setdefault(name, set())

    def _ensure_list(self, name):
        return self.result.setdefault(name, [])

    def add_attribute(self, idl_attribute):
        self._ensure_list('attributes').append(
            self.create_attribute(idl_attribute))
        self._ensure_set('cpp_includes').update(
            self.type_resolver.includes_from_definition(idl_attribute))

    def add_operation(self, idl_operation):
        if not idl_operation.name:
            return
        self._ensure_list('methods').append(
            self.create_method(idl_operation))
        self._ensure_set('cpp_includes').update(
            self.type_resolver.includes_from_definition(idl_operation))

    def create_method(self, idl_operation):
        name = idl_operation.name
        return_type = self.type_resolver.type_from_definition(idl_operation)
        return {
            'name': name,
            'return_type': return_type
        }

    def create_attribute(self, idl_attribute):
        name = idl_attribute.name
        return_type = self.type_resolver.type_from_definition(idl_attribute)
        return {
            'name': name,
            'return_type': return_type
        }

    def build(self):
        return self.result


class CodeGeneratorWebModule(CodeGeneratorBase):
    def __init__(self, info_provider, cache_dir, output_dir):
        CodeGeneratorBase.__init__(self, MODULE_PYNAME, info_provider,
                                   cache_dir, output_dir)
        self.typedef_resolver = TypedefResolver(info_provider)

    def get_template(self, file_extension):
        template_filename = 'web_module_interface.%s.tmpl' % file_extension
        return self.jinja_env.get_template(template_filename)

    # TODO(dglazkov): Move to CodeGeneratorBase.
    def output_paths(self, definition_name):
        header_path = posixpath.join(self.output_dir,
                                     'Web%s.h' % definition_name)
        cpp_path = posixpath.join(self.output_dir,
                                  'Web%s.cpp' % definition_name)
        return header_path, cpp_path

    def generate_interface_code(self, interface):
        # TODO(dglazkov): Implement callback interfaces.
        # TODO(dglazkov): Make sure partial interfaces are handled.
        if interface.is_callback or interface.is_partial:
            raise ValueError('Partial or callback interfaces are not supported')

        template_context = interface_context(interface)

        cpp_template = self.get_template('cpp')
        header_template = self.get_template('h')
        cpp_text = render_template(cpp_template, template_context)
        header_text = render_template(header_template, template_context)
        header_path, cpp_path = self.output_paths(interface.name)

        return (
            (header_path, header_text),
            (cpp_path, cpp_text)
        )

    def generate_code(self, definitions, definition_name):
        self.typedef_resolver.resolve(definitions, definition_name)
        header_path, cpp_path = self.output_paths(definition_name)

        template_context = {}
        # TODO(dglazkov): Implement dictionaries
        if definition_name not in definitions.interfaces:
            return None

        interface = definitions.interfaces[definition_name]
        if WEB_MODULE_IDL_ATTRIBUTE not in interface.extended_attributes:
            return None

        return self.generate_interface_code(interface)
