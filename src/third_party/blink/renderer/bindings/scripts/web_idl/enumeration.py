# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .idl_definition import IdlDefinition


class Enumeration(IdlDefinition):
    """https://heycam.github.io/webidl/#idl-enums"""

    @property
    def values(self):
        """
        Returns a list of values listed in this enumeration.
        @return tuple(str)
        """
        raise exceptions.NotImplementedError()
