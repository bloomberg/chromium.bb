# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

try:
    import _pickle as pickle
except ImportError:
    try:
        import cPickle as pickle
    except ImportError:
        import pickle

import idl_parser
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


def load_and_register_idl_definitions(
        filepaths, register_ir, create_ref_to_idl_type, create_ref_to_idl_def):
    """
    Reads ASTs from |filepaths| and builds IRs from ASTs.

    Args:
        filepaths: Paths to pickle files that store AST nodes.
        register_ir: A callback function that registers the argument as an
            IR.
        create_ref_to_idl_type: A callback function that creates a reference
            to an IdlType from the given identifier.
        create_ref_to_idl_def: A callback function that creates a reference
            to an IDL definition from the given identifier.
    """
    assert callable(register_ir)

    for filepath in filepaths:
        with open(filepath) as pickle_file:
            asts_per_component = pickle.load(pickle_file)
            component = asts_per_component.component
            builder = _IRBuilder(component, create_ref_to_idl_type,
                                 create_ref_to_idl_def)

            for file_node in asts_per_component.asts:
                assert isinstance(file_node, idl_parser.idl_node.IDLNode)
                assert file_node.GetClass() == 'File'

                for top_level_node in file_node.GetChildren():
                    register_ir(builder.build_top_level_def(top_level_node))


class _IRBuilder(object):
    def __init__(self, component, create_ref_to_idl_type,
                 create_ref_to_idl_def):
        """
        Args:
            component: A Component to which the built IRs are associated.
            create_ref_to_idl_type: A callback function that creates a reference
                to an IdlType from the given identifier.
            create_ref_to_idl_def: A callback function that creates a reference
                to an IDL definition from the given identifier.
        """
        assert callable(create_ref_to_idl_type)
        assert callable(create_ref_to_idl_def)

        self.component = component
        self.create_ref_to_idl_type = create_ref_to_idl_type
        self.create_ref_to_idl_def = create_ref_to_idl_def

    def build_top_level_def(self, node):
        build_functions = {
            'Callback': self._build_callback_function,
            'Dictionary': self._build_dictionary,
            'Enum': self._build_enumeration,
            'Includes': self._build_includes,
            'Interface': self._build_interface,
            'Namespace': self._build_namespace,
            'Typedef': self._build_typedef,
        }
        return build_functions[node.GetClass()](node)

    # Builder functions for top-level definitions

    def _build_interface(self, node):
        if node.GetProperty('CALLBACK'):
            return self._build_callback_interface(node)

        interface = Interface.IR(
            identifier=node.GetName(),
            is_partial=bool(node.GetProperty('PARTIAL')),
            is_mixin=bool(node.GetProperty('MIXIN')),
            component=self.component,
            debug_info=self._build_debug_info(node))
        # TODO(peria): Build members and register them in |interface|
        return interface

    def _build_namespace(self, node):
        namespace = Namespace.IR(
            identifier=node.GetName(),
            is_partial=bool(node.GetProperty('PARTIAL')),
            component=self.component,
            debug_info=self._build_debug_info(node))
        # TODO(peria): Build members and register them in |namespace|
        return namespace

    def _build_dictionary(self, node):
        child_nodes = list(node.GetChildren())
        extended_attributes = self._take_extended_attributes(child_nodes)
        # TODO(yukishiino): Implement dictionary inheritance.
        _ = self._take_inheritance(child_nodes)
        own_members = [
            self._build_dictionary_member(child) for child in child_nodes
        ]

        dictionary = Dictionary.IR(
            identifier=node.GetName(),
            is_partial=bool(node.GetProperty('PARTIAL')),
            own_members=own_members,
            extended_attributes=extended_attributes,
            component=self.component,
            debug_info=self._build_debug_info(node))
        return dictionary

    def _build_dictionary_member(self, node):
        assert node.GetClass() == 'Key'

        child_nodes = list(node.GetChildren())
        idl_type = self._take_type(child_nodes)
        default_value = self._take_default_value(child_nodes)
        extended_attributes = self._take_extended_attributes(child_nodes)
        assert len(child_nodes) == 0

        return DictionaryMember.IR(
            identifier=node.GetName(),
            idl_type=idl_type,
            is_required=bool(node.GetProperty('REQUIRED')),
            default_value=default_value,
            extended_attributes=extended_attributes,
            component=self.component,
            debug_info=self._build_debug_info(node))

    def _build_callback_interface(self, node):
        callback_interface = CallbackInterface.IR(
            identifier=node.GetName(),
            component=self.component,
            debug_info=self._build_debug_info(node))
        # TODO(peria): Build members and register them in |callback_interface|
        return callback_interface

    def _build_callback_function(self, node):
        callback_function = CallbackFunction.IR(
            identifier=node.GetName(),
            component=self.component,
            debug_info=self._build_debug_info(node))
        # TODO(peria): Build members and register them in |callback_function|
        return callback_function

    def _build_enumeration(self, node):
        enumeration = Enumeration.IR(
            identifier=node.GetName(),
            values=[child.GetName() for child in node.GetChildren()],
            component=self.component,
            debug_info=self._build_debug_info(node))
        return enumeration

    def _build_typedef(self, node):
        child_nodes = list(node.GetChildren())
        idl_type = self._take_type(child_nodes)
        assert len(child_nodes) == 0

        typedef = Typedef.IR(
            identifier=node.GetName(),
            idl_type=idl_type,
            component=self.component,
            debug_info=self._build_debug_info(node))
        return typedef

    def _build_includes(self, node):
        includes = Includes.IR(
            interface_identifier=node.GetName(),
            mixin_identifier=node.GetProperty('REFERENCE'),
            component=self.component,
            debug_info=self._build_debug_info(node))
        return includes

    # Helper functions sorted alphabetically

    def _build_debug_info(self, node):
        return DebugInfo(
            location=DebugInfo.Location(
                filepath=node.GetProperty('FILENAME'),
                line_number=node.GetProperty('LINENO'),
                column_number=node.GetProperty('POSITION')))

    def _build_default_value(self, node):
        assert node.GetClass() == 'Default'
        return DefaultValue()

    def _build_extended_attributes(self, node):
        assert node.GetClass() == 'ExtAttributes'
        return ExtendedAttributes()

    def _build_inheritance(self, node):
        assert node.GetClass() == 'Inherit'
        return None

    def _build_type(self, node):
        assert node.GetClass() == 'Type'
        return IdlType()

    def _take_and_build(self, node_class, build_func, node_list):
        """
        Takes a node of |node_class| from |node_list| if any, and then builds
        and returns an IR.  The processed node is removed from |node_list|.
        Returns None if not found.
        """
        for node in node_list:
            if node.GetClass() == node_class:
                node_list.remove(node)
                return build_func(node)
        return None

    def _take_default_value(self, node_list):
        return self._take_and_build('Default', self._build_default_value,
                                    node_list)

    def _take_extended_attributes(self, node_list):
        return self._take_and_build('ExtAttributes',
                                    self._build_extended_attributes, node_list)

    def _take_inheritance(self, node_list):
        return self._take_and_build('Inherit', self._build_inheritance,
                                    node_list)

    def _take_type(self, node_list):
        return self._take_and_build('Type', self._build_type, node_list)
