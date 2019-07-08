# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import idl_parser
import utilities
from .common import DebugInfo
from .interface import Interface
from .namespace import Namespace
from .dictionary import Dictionary
from .callback_interface import CallbackInterface
from .callback_function import CallbackFunction
from .enumeration import Enumeration
from .typedef import Typedef
from .includes import Includes


def load_and_register_idl_definitions(filepaths, ir_map):
    """
    Registers IDL definitions' IRs into a IDL compiler

    Load ASTs from files, create IRs, and registers them into |ir_map|.
    """
    asts_grouped_by_component = utilities.read_pickle_files(filepaths)
    for asts_per_component in asts_grouped_by_component:
        component = asts_per_component.component
        for file_node in asts_per_component.asts:
            assert isinstance(file_node, idl_parser.idl_node.IDLNode)
            for definition_node in file_node.GetChildren():
                idl_definition = _convert_ast(component, definition_node)
                ir_map.register(idl_definition)


def _convert_ast(component, ast):
    def build_interface(node):
        if node.GetProperty('CALLBACK'):
            return build_callback_interface(node)

        identifier = node.GetName()
        filepath, line_number = node.GetFileAndLine()
        is_partial = bool(node.GetProperty('PARTIAL'))
        is_mixin = bool(node.GetProperty('MIXIN'))
        interface = Interface.IR(
            identifier=identifier,
            is_partial=is_partial,
            is_mixin=is_mixin,
            component=component,
            debug_info=DebugInfo(filepath=filepath, line_number=line_number))
        # TODO(peria): Build members and register them in |interface|
        return interface

    def build_namespace(node):
        identifier = node.GetName()
        filepath, line_number = node.GetFileAndLine()
        is_partial = bool(node.GetProperty('PARTIAL'))
        namespace = Namespace.IR(
            identifier=identifier,
            is_partial=is_partial,
            component=component,
            debug_info=DebugInfo(filepath=filepath, line_number=line_number))
        # TODO(peria): Build members and register them in |namespace|
        return namespace

    def build_dictionary(node):
        identifier = node.GetName()
        filepath, line_number = node.GetFileAndLine()
        is_partial = bool(node.GetProperty('PARTIAL'))
        dictionary = Dictionary.IR(
            identifier=identifier,
            is_partial=is_partial,
            component=component,
            debug_info=DebugInfo(filepath=filepath, line_number=line_number))
        # TODO(peria): Build members and register them in |dictionary|
        return dictionary

    def build_callback_interface(node):
        identifier = node.GetName()
        filepath, line_number = node.GetFileAndLine()
        callback_interface = CallbackInterface.IR(
            identifier=identifier,
            component=component,
            debug_info=DebugInfo(filepath=filepath, line_number=line_number))
        # TODO(peria): Build members and register them in |callback_interface|
        return callback_interface

    def build_callback_function(node):
        identifier = node.GetName()
        filepath, line_number = node.GetFileAndLine()
        callback_function = CallbackFunction.IR(
            identifier=identifier,
            component=component,
            debug_info=DebugInfo(filepath=filepath, line_number=line_number))
        # TODO(peria): Build members and register them in |callback_function|
        return callback_function

    def build_enumeration(node):
        identifier = node.GetName()
        filepath, line_number = node.GetFileAndLine()
        enumeration = Enumeration.IR(
            identifier=identifier,
            component=component,
            debug_info=DebugInfo(filepath=filepath, line_number=line_number))
        # TODO(peria): Build members and register them in |enumeration|
        return enumeration

    def build_typedef(node):
        identifier = node.GetName()
        filepath, line_number = node.GetFileAndLine()
        typedef = Typedef.IR(
            identifier=identifier,
            component=component,
            debug_info=DebugInfo(filepath=filepath, line_number=line_number))
        # TODO(peria): Build members and register them in |typedef|
        return typedef

    def build_includes(node):
        interface_identifier = node.GetName()
        filepath, line_number = node.GetFileAndLine()
        includes = Includes.IR(
            interface_identifier=interface_identifier,
            component=component,
            debug_info=DebugInfo(filepath=filepath, line_number=line_number))
        # TODO(peria): Build members and register them in |includes|
        return includes

    def dispatch_to_build_function(node):
        node_class = node.GetClass()
        assert node_class in build_functions, (
            '{} is unknown node class.'.format(node_class))
        return build_functions[node_class](node)

    build_functions = {
        'Callback': build_callback_function,
        'Dictionary': build_dictionary,
        'Enum': build_enumeration,
        'Includes': build_includes,
        'Interface': build_interface,
        'Namespace': build_namespace,
        'Typedef': build_typedef,
    }
    return dispatch_to_build_function(ast)
