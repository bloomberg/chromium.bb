# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .common import WithComponent
from .common import WithDebugInfo


class Includes(WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#include"""

    @property
    def interface(self):
        """
        Returns the interface that includes the mixin.
        @return Interface
        """
        raise exceptions.NotImplementedError()

    @property
    def mixin(self):
        """
        Returns the interface mixin that is included by the other.
        @return Interface
        """
        raise exceptions.NotImplementedError()
