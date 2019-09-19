# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .attribute import Attribute
from .code_generator_info import CodeGeneratorInfo
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExposure
from .composition_parts import WithExtendedAttributes
from .constant import Constant
from .exposure import Exposure
from .ir_map import IRMap
from .make_copy import make_copy
from .operation import Operation
from .operation import OperationGroup
from .user_defined_type import UserDefinedType


class Namespace(UserDefinedType, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithExposure, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-namespaces"""

    class IR(IRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     is_partial,
                     attributes=None,
                     constants=None,
                     operations=None,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            assert isinstance(is_partial, bool)
            assert attributes is None or isinstance(attributes, (list, tuple))
            assert constants is None or isinstance(constants, (list, tuple))
            assert operations is None or isinstance(operations, (list, tuple))

            attributes = attributes or []
            constants = constants or []
            operations = operations or []
            assert all(
                isinstance(attribute, Attribute.IR) and attribute.is_readonly
                and attribute.is_static for attribute in attributes)
            assert all(
                isinstance(constant, Constant.IR) for constant in constants)
            assert all(
                isinstance(operation, Operation.IR) and operation.identifier
                and operation.is_static for operation in operations)

            kind = (IRMap.IR.Kind.PARTIAL_NAMESPACE
                    if is_partial else IRMap.IR.Kind.NAMESPACE)
            IRMap.IR.__init__(self, identifier=identifier, kind=kind)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithComponent.__init__(self, component=component)
            WithDebugInfo.__init__(self, debug_info)

            self.is_partial = is_partial
            self.attributes = list(attributes)
            self.constants = list(constants)
            self.operations = list(operations)
            self.operation_groups = []

    def __init__(self, ir):
        assert isinstance(ir, Namespace.IR)
        assert not ir.is_partial

        ir = make_copy(ir)
        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, ir.extended_attributes)
        WithCodeGeneratorInfo.__init__(
            self, CodeGeneratorInfo(ir.code_generator_info))
        WithExposure.__init__(self, Exposure(ir.exposure))
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info)

        self._attributes = tuple([
            Attribute(attribute_ir, owner=self)
            for attribute_ir in ir.attributes
        ])
        self._constants = tuple([
            Constant(constant_ir, owner=self) for constant_ir in ir.constants
        ])
        self._operations = tuple([
            Operation(operation_ir, owner=self)
            for operation_ir in ir.operations
        ])
        self._operation_groups = tuple([
            OperationGroup(
                operation_group_ir,
                filter(lambda x: x.identifier == operation_group_ir.identifier,
                       self._operations),
                owner=self) for operation_group_ir in ir.operation_groups
        ])

    @property
    def attributes(self):
        """Returns attributes."""
        return self._attributes

    @property
    def operations(self):
        """Returns operations."""
        return self._operations

    @property
    def operation_groups(self):
        """Returns a list of OperationGroups."""
        return self._operation_groups
