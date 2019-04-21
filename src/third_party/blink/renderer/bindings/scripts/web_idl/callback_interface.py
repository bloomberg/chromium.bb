# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .idl_definition import IdlDefinition


class CallbackInterface(IdlDefinition):
    """https://heycam.github.io/webidl/#idl-interfaces"""

    @property
    def inherited_callback_interface(self):
        """
        Returns an CallbackInterface which this interface is inherited from.
        @return CallbackInterface?
        """
        raise exceptions.NotImplementedError()

    @property
    def operation_groups(self):
        """
        Returns a list of OperationGroup. Each OperationGroup may have an
        operation or a set of overloaded operations.
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
