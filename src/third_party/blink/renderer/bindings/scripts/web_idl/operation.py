# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithIdentifier
from .common import WithOwner
from .idl_member import IdlMember


class Operation(IdlMember):
    """https://heycam.github.io/webidl/#idl-operations
    https://www.w3.org/TR/WebIDL-1/#idl-special-operations"""

    @property
    def is_static(self):
        """
        Returns True if 'static' is specified.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def return_type(self):
        """
        Returns the type of return value.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    @property
    def arguments(self):
        """
        Returns a list of arguments.
        @return tuple(Argument)
        """
        raise exceptions.NotImplementedError()

    @property
    def overloaded_index(self):
        """
        Returns the index in the OperationGroup that |self| belongs to.
        @return int
        """
        raise exceptions.NotImplementedError()


class OperationGroup(WithIdentifier, WithCodeGeneratorInfo, WithOwner,
                     WithComponent, WithDebugInfo):
    """
    OperationGroup class has all Operation's with a same identifier, even if the
    operation is not overloaded. Then we can handle overloaded and
    non-overloaded operations seamlessly.
    From the ES bindings' view point, OperationGroup tells something for properties,
    and Operation tells something for actual behaviors.
    """

    def operations(self):
        """
        Returns a list of operations whose identifier is |identifier()|
        @return tuple(Operation)
        """
        raise exceptions.NotImplementedError()
