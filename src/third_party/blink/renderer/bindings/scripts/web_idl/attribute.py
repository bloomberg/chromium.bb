# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .idl_member import IdlMember


class Attribute(IdlMember):
    """https://heycam.github.io/webidl/#idl-attributes"""

    @property
    def idl_type(self):
        """
        Returns type of this attribute.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    @property
    def is_static(self):
        """
        Returns True if this attriute is static.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_readonly(self):
        """
        Returns True if this attribute is read only.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def does_inherit_getter(self):
        """
        Returns True if |self| inherits its getter.
        https://heycam.github.io/webidl/#dfn-inherit-getter
        @return bool
        """
        raise exceptions.NotImplementedError()
