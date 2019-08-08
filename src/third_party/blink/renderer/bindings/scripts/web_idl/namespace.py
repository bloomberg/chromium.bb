# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .idl_definition import IdlDefinition


class Namespace(IdlDefinition):
    """https://heycam.github.io/webidl/#idl-namespaces"""

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
