# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .attribute import Attribute
from .code_generator_info import CodeGeneratorInfoMutable
from .composition_parts import Identifier
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithIdentifier
from .idl_type import IdlType


class ObservableArray(WithIdentifier, WithCodeGeneratorInfo, WithComponent,
                      WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-observable-array"""

    def __init__(self, idl_type, attributes):
        assert isinstance(idl_type, IdlType)
        assert isinstance(attributes, (list, tuple)) and all(
            isinstance(attribute, Attribute) for attribute in attributes)
        assert idl_type.is_observable_array

        identifier = Identifier('ObservableArray_{}'.format(
            (idl_type.element_type.type_name_with_extended_attribute_key_values
             )))

        components = []
        element_type = idl_type.element_type.unwrap()
        component_object = (element_type.type_definition_object
                            or element_type.union_definition_object)
        if component_object:
            components.append(component_object.components[0])

        for_testing = [True]
        for attribute in attributes:
            interface = attribute.owner
            if not interface.code_generator_info.for_testing:
                for_testing[0] = False

        code_generator_info = CodeGeneratorInfoMutable()
        code_generator_info.set_for_testing(for_testing[0])

        WithIdentifier.__init__(self, identifier)
        WithCodeGeneratorInfo.__init__(self,
                                       code_generator_info,
                                       readonly=True)
        WithComponent.__init__(self, components, readonly=True)
        WithDebugInfo.__init__(self)

        self._idl_type = idl_type
        self._user_attributes = tuple(attributes)

        for attribute in self._user_attributes:
            idl_type = attribute.idl_type.unwrap()
            idl_type.set_observable_array_definition_object(self)

    @property
    def idl_type(self):
        """Returns the type of the observable array."""
        return self._idl_type

    @property
    def element_type(self):
        """Returns the element type of the observable array."""
        return self.idl_type.element_type

    @property
    def user_attributes(self):
        """Returns the attributes that use this observable array type."""
        return self._user_attributes
