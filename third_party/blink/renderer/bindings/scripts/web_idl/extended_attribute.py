# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file defines some classes to implement extended attributes
# https://heycam.github.io/webidl/#idl-extended-attributes

import exceptions


class ExtendedAttribute(object):
    """ExtendedAttribute represents an exnteded attribute, except for attributes
    that have their own classes.
    i.e. [Constructor], [CustomConstructor], [NamedConstructor], and [Exposed].
    ExtendedAttribute assumes following formats as spec defines.
    a. [Key]
    b. [Key=Value]
    c. [Key=(Value1,Value2,...)]
    d. [Key(Type1 Value1, Type2 Value2,...)]
    e. [Key=Name(Type1 Value1, Type2 Value2,...)]
    """

    @property
    def key(self):
        """
        Returns the key.
        @return str
        """
        raise exceptions.NotImplementedError()

    @property
    def value(self):
        """
        Returns the value for (b). Returns None for (a). Crashes otherwise.
        @return str
        """
        raise exceptions.NotImplementedError()

    @property
    def values(self):
        """
        Returns a list of values for (b) and (c). Returns an empty list
        for (a). Crashes otherwise.
        @return tuple(str)
        """
        raise exceptions.NotImplementedError()

    @property
    def arguments(self):
        """
        Returns a tuple of arguments for (d) and (e). Crashes otherwise.
        @return tuple(Argument)
        """
        raise exceptions.NotImplementedError()

    @property
    def name(self):
        """
        Returns |Name| for (e). Crashes otherwise.
        @return str
        """
        raise exceptions.NotImplementedError()


class ExtendedAttributes(object):
    """ExtendedAttributes is a dict-like container for ExtendedAttribute instances."""

    def has_key(self, _):
        """
        Returns True if this has an extended attribute with the |key|.
        @param  str key
        @return bool
        """
        raise exceptions.NotImplementedError()

    def get(self, _):
        """
        Returns an exnteded attribute whose key is |key|.
        If |self| has no such elements, returns None,
        and if |self| has multiple elements, raises an error.
        @param  str key
        @return ExtendedAttriute?
        """
        raise exceptions.NotImplementedError()

    def get_list_of(self, _):
        """
        Returns a list of extended attributes whose keys are |identifier|.
        @param  str key
        @return tuple(ExtendedAttribute)
        """
        raise exceptions.NotImplementedError()
