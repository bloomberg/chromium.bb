# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import idl_parser
import utilities
from .callback_function import CallbackFunction
from .callback_interface import CallbackInterface
from .common import DebugInfo
from .dictionary import Dictionary
from .dictionary import DictionaryMember
from .enumeration import Enumeration
from .extended_attribute import ExtendedAttributes
from .idl_types import IdlType
from .includes import Includes
from .interface import Interface
from .namespace import Namespace
from .typedef import Typedef
from .values import DefaultValue


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
            assert file_node.GetClass() == 'File'
            for top_level_def in file_node.GetChildren():
                idl_definition = _build_top_level_def(component, top_level_def)
                ir_map.register(idl_definition)


def _build_top_level_def(component, node):
    def build_interface(node):
        if node.GetProperty('CALLBACK'):
            return build_callback_interface(node)

        interface = Interface.IR(
            identifier=node.GetName(),
            is_partial=bool(node.GetProperty('PARTIAL')),
            is_mixin=bool(node.GetProperty('MIXIN')),
            component=component,
            debug_info=_build_debug_info(node))
        # TODO(peria): Build members and register them in |interface|
        return interface

    def build_namespace(node):
        namespace = Namespace.IR(
            identifier=node.GetName(),
            is_partial=bool(node.GetProperty('PARTIAL')),
            component=component,
            debug_info=_build_debug_info(node))
        # TODO(peria): Build members and register them in |namespace|
        return namespace

    def build_dictionary(node):
        def build_dictionary_member(node):
            assert node.GetClass() == 'Key'

            child_nodes = list(node.GetChildren())
            idl_type = _take_type(child_nodes)
            default_value = _take_default_value(child_nodes)
            extended_attributes = _take_extended_attributes(child_nodes)
            assert len(child_nodes) == 0

            return DictionaryMember.IR(
                identifier=node.GetName(),
                idl_type=idl_type,
                is_required=bool(node.GetProperty('REQUIRED')),
                default_value=default_value,
                extended_attributes=extended_attributes,
                component=component,
                debug_info=_build_debug_info(node))

        child_nodes = list(node.GetChildren())
        extended_attributes = _take_extended_attributes(child_nodes)
        _ = _take_inheritance(child_nodes)
        own_members = [build_dictionary_member(child) for child in child_nodes]

        dictionary = Dictionary.IR(
            identifier=node.GetName(),
            is_partial=bool(node.GetProperty('PARTIAL')),
            own_members=own_members,
            extended_attributes=extended_attributes,
            component=component,
            debug_info=_build_debug_info(node))
        return dictionary

    def build_callback_interface(node):
        callback_interface = CallbackInterface.IR(
            identifier=node.GetName(),
            component=component,
            debug_info=_build_debug_info(node))
        # TODO(peria): Build members and register them in |callback_interface|
        return callback_interface

    def build_callback_function(node):
        callback_function = CallbackFunction.IR(
            identifier=node.GetName(),
            component=component,
            debug_info=_build_debug_info(node))
        # TODO(peria): Build members and register them in |callback_function|
        return callback_function

    def build_enumeration(node):
        enumeration = Enumeration.IR(
            identifier=node.GetName(),
            values=[child.GetName() for child in node.GetChildren()],
            component=component,
            debug_info=_build_debug_info(node))
        return enumeration

    def build_typedef(node):
        typedef = Typedef.IR(
            identifier=node.GetName(),
            idl_type=dispatch_to_build_function(node.GetChildren()[0]),
            component=component,
            debug_info=_build_debug_info(node))
        return typedef

    def build_includes(node):
        includes = Includes.IR(
            interface_identifier=node.GetName(),
            mixin_identifier=node.GetProperty('REFERENCE'),
            component=component,
            debug_info=_build_debug_info(node))
        return includes

    def dispatch_to_build_function(node):
        node_class = node.GetClass()
        # TODO(peria): Drop this if branch returning None, when all the build
        # functions get implemented.  It is only to avoid a build error in CQ.
        if node_class not in build_functions:
            return None
        assert node_class in build_functions, '{} is unknown node class.'.format(
            node_class)
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
    return dispatch_to_build_function(node)


def _build_debug_info(node):
    return DebugInfo(
        location=DebugInfo.Location(
            filepath=node.GetProperty('FILENAME'),
            line_number=node.GetProperty('LINENO'),
            column_number=node.GetProperty('POSITION')))


def _build_default_value(node):
    assert node.GetClass() == 'Default'
    return DefaultValue()


def _build_extended_attributes(node):
    assert node.GetClass() == 'ExtAttributes'
    return ExtendedAttributes()


def _build_inheritance(node):
    assert node.GetClass() == 'Inherit'
    return None


def _build_type(node):
    assert node.GetClass() == 'Type'
    return IdlType()


def _take_and_build(node_class, build_func, node_list):
    for node in node_list:
        if node.GetClass() == node_class:
            node_list.remove(node)
            return build_func(node)
    return None


def _take_default_value(node_list):
    return _take_and_build('Default', _build_default_value, node_list)


def _take_extended_attributes(node_list):
    return _take_and_build('ExtAttributes', _build_extended_attributes,
                           node_list)


def _take_inheritance(node_list):
    return _take_and_build('Inherit', _build_inheritance, node_list)


def _take_type(node_list):
    return _take_and_build('Type', _build_type, node_list)
