# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions

# Exposure is a part of ExtendedAttribute in a concept, but it should be
# accessible from other classes easily.


class Exposure(object):
    @property
    def global_interfaces(self):
        """
        Returns the global interface to be visible in.
        @return tuple(Interface)
        """
        raise exceptions.NotImplementedError()

    @property
    def runtime_enabled_flags(self):
        """
        Returns a list of runtime enabled featuers.
        @return tuple(str)
        """
        raise exceptions.NotImplementedError()

    @property
    def origin_trials(self):
        """
        Returns a list of origin trial features.
        @return tuple(str)
        """
        raise exceptions.NotImplementedError()

    @property
    def is_secure_context(self):
        """
        Return true if the exposure requires secure context.
        @return bool
        """
        raise exceptions.NotImplementedError()
