# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExposure
from .common import WithExtendedAttributes
from .common import WithIdentifier


class IdlDefinition(WithIdentifier, WithExtendedAttributes, WithExposure,
                    WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
    """
    IdlDefinition provides common APIs for IDL definitions; Interface, Dictionary,
    Namespace, CallbackFunction, CallbackInterface, and Enumeration.
    """

    @property
    def is_interface(self):
        """
        Returns True if |self| is an Interface.
        @return bool
        """
        return False

    @property
    def is_dictionary(self):
        """
        Returns True if |self| is a Dictionary.
        @return bool
        """
        return False

    @property
    def is_namespace(self):
        """
        Returns True if |self| is a Namespace.
        @return bool
        """
        return False

    @property
    def is_callback_function(self):
        """
        Returns True if |self| is a CallbackFunction.
        @return bool
        """
        return False

    @property
    def is_callback_interface(self):
        """
        Returns True if |self| is a CallbackInterface.
        @return bool
        """
        return False

    @property
    def is_enumeration(self):
        """
        Returns True if |self| is an Enumeration.
        @return bool
        """
        return False
