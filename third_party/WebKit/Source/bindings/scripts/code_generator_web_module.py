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

from code_generator import CodeGeneratorBase
# TODO(dglazkov): Move TypedefResolver to code_generator.py
from code_generator_v8 import TypedefResolver

MODULE_PYNAME = os.path.splitext(os.path.basename(__file__))[0] + '.py'

WEB_MODULE_IDL_ATTRIBUTE = 'WebModuleAPI'


def interface_context(idl_interface):
    builder = InterfaceContextBuilder(MODULE_PYNAME)
    builder.set_class_name(idl_interface.name)
    builder.set_inheritance(idl_interface.parent)

    for idl_attribute in idl_interface.attributes:
        builder.add_attribute(idl_attribute)

    for idl_operation in idl_interface.operations:
        builder.add_operation(idl_operation)

    return builder.build()


class InterfaceContextBuilder(object):
    def __init__(self, code_generator):
        self.result = {'code_generator': code_generator}

    def set_class_name(self, class_name):
        self.result['class_name'] = class_name

    def set_inheritance(self, base_interface):
        if base_interface is None:
            return
        self.result['inherits_expression'] = ' : public %s' % base_interface
        self._ensure_set('cpp_includes').update(
            self._includes_for_type(base_interface))

    def _ensure_set(self, name):
        return self.result.setdefault(name, set())

    def _ensure_list(self, name):
        return self.result.setdefault(name, [])

    def _includes_for_type(self, idl_type):
        # TODO(dglazkov): Make this actually work.
        name = idl_type
        return set([name])

    def _get_return_type(self, idl_definition):
        return idl_definition.idl_type.preprocessed_type.base_type

    def add_attribute(self, idl_attribute):
        self._ensure_list('attributes').append(
            self.create_attribute(idl_attribute))
        self._ensure_set('cpp_includes').update(
            self._includes_for_type(self._get_return_type(idl_attribute)))

    def add_operation(self, idl_operation):
        if not idl_operation.name:
            return
        self._ensure_list('methods').append(
            self.create_method(idl_operation))
        self._ensure_set('cpp_includes').update(
            self._includes_for_type(self._get_return_type(idl_operation)))

    def create_method(self, idl_operation):
        name = idl_operation.name
        return_type = idl_operation.idl_type.preprocessed_type.base_type
        return {
            'name': name,
            'return_type': return_type
        }

    def create_attribute(self, idl_attribute):
        name = idl_attribute.name
        return_type = idl_attribute.idl_type.preprocessed_type.base_type
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
        cpp_text = cpp_template.render(template_context)
        header_text = header_template.render(template_context)
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
