# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions

from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithIdentifier
from .ir_map import IRMap


class Namespace(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-namespaces"""

    class IR(IRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     is_partial,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            kind = (IRMap.IR.Kind.PARTIAL_NAMESPACE
                    if is_partial else IRMap.IR.Kind.NAMESPACE)
            IRMap.IR.__init__(self, identifier=identifier, kind=kind)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

    @property
    def attributes(self):
        """
        Returns a list of attributes.
        @return tuple(Attribute)
        """
        raise exceptions.NotImplementedError()

    @property
    def operation_groups(self):
        """
        Returns a list of OperationGroup. Each OperationGroup may have an operation
        or a set of overloaded operations.
        @return tuple(OperationGroup)
        """
        raise exceptions.NotImplementedError()

    @property
    def constants(self):
        """
        Returns a list of constants.
        @return tuple(Constant)
        """
        raise exceptions.NotImplementedError()
