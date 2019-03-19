# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithIdentifier


class Typedef(WithIdentifier, WithCodeGeneratorInfo, WithComponent,
              WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-typedefs"""

    @property
    def idl_type(self):
        """
        Returns the type to have an alias.
        @return IdlType
        """
        raise exceptions.NotImplementedError()
