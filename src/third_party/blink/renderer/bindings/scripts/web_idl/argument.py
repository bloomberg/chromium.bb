# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .common import WithIdentifier
from .common import WithExtendedAttributes
from .common import WithCodeGeneratorInfo
from .common import WithOwner


class Argument(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
               WithOwner):
    @property
    def idl_type(self):
        """
        Returns type of this argument.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    @property
    def is_optional(self):
        """
        Returns True if this argument is optional.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_variadic(self):
        """
        Returns True if this argument is variadic.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def default_value(self):
        """
        Returns the default value if it is specified. Otherwise, None
        @return DefaultValue
        """
        raise exceptions.NotImplementedError()

    @property
    def index(self):
        """
        Returns its index in an operation's arguments
        @return int
        """
        raise exceptions.NotImplementedError()
