# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .idl_definition import IdlDefinition


class CallbackFunction(IdlDefinition):
    """https://heycam.github.io/webidl/#idl-callback-functions"""

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
        @return Argument
        """
        raise exceptions.NotImplementedError()
