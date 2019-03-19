# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .idl_member import IdlMember


class Constant(IdlMember):
    """https://heycam.github.io/webidl/#idl-constants"""

    @property
    def idl_type(self):
        """
        Returns the type of this constant.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    @property
    def value(self):
        """
        Returns the constant value.
        @return ConstantValue
        """
        raise exceptions.NotImplementedError()
