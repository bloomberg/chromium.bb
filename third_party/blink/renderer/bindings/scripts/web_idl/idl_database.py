# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions


class IdlDatabase(object):
    @property
    def interfaces(self):
        """
        Returns a list of non callback interfaces.
        @return tuple(Interface)
        """
        raise exceptions.NotImplementedError()

    @property
    def dictionaries(self):
        """
        Returns a list of dictionaries.
        @return tuple(Dictionary)
        """
        raise exceptions.NotImplementedError()

    @property
    def namespaces(self):
        """
        Returns a list of namespaces.
        @return tuple(Namespace)
        """
        raise exceptions.NotImplementedError()

    @property
    def callback_functions(self):
        """
        Returns a list of callback functions.
        @return tuple(CallbackFunction)
        """
        raise exceptions.NotImplementedError()

    @property
    def callback_interfaces(self):
        """
        Returns a list of callback interfaces.
        @return tuple(CallbackInterface)
        """
        raise exceptions.NotImplementedError()

    @property
    def typedefs(self):
        """
        Returns a list of typedefs.
        @return tuple(Typedef)
        """
        raise exceptions.NotImplementedError()

    @property
    def union_types(self):
        """
        Returns a list of union types.
        @return tuple(IdlUnionType)
        """
        raise exceptions.NotImplementedError()
