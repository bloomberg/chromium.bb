# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions


class DefaultValue(object):
    def idl_type(self):
        """
        Returns either of string, number, boolean(true/false), null, and sequence[].
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    def value(self):
        """
        Returns the default value. Actual type depends on the value itself.
        @return object(TBD)
        """
        raise exceptions.NotImplementedError()


class ConstantValue(object):
    def idl_type(self):
        """
        Returns either of string, number, boolean(true/false), null, and sequence[].
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    def value(self):
        """
        Returns the default value. Actual type depends on the value itself.
        @return object(TBD)
        """
        raise exceptions.NotImplementedError()
