# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions

Identifier = str


class WithIdentifier(object):
    """WithIdentifier class is an interface that indicates the class has an
    identifier."""

    @property
    def identifier(self):
        """
        Returns the identifier.
        @return Identifier
        """
        raise exceptions.NotImplementedError()


# ExtendedAttribute and ExtendedAttributes are defined in extended_attribute.py
class WithExtendedAttributes(object):
    """WithExtendedAttributes class is an interface that indicates the implemented
    class can have extended attributes."""

    @property
    def extended_attributes(self):
        """
        Returns the extended attributes.
        @return ExtendedAttributes
        """
        raise exceptions.NotImplementedError()


class WithCodeGeneratorInfo(object):
    """WithCodeGeneratorInfo class is an interface that its inheritances can
    provide some information for code generators."""

    @property
    def code_generator_info(self):
        """
        Returns information for code generator.
        @return dict(TBD)
        """
        raise exceptions.NotImplementedError()


class WithExposure(object):
    """WithExposure class is an interface that its inheritances can have Exposed
    extended attributes."""

    @property
    def exposures(self):
        """
        Returns a set of Exposure's that are applicable on an object.
        https://heycam.github.io/webidl/#Exposed
        @return tuple(Expsure)
        """
        raise exceptions.NotImplementedError()


Component = str


class WithComponent(object):
    """WithComponent class is an interface to show which components this
    object belongs to."""

    # The order of |_COMPONENTS| shows the order of their dependencies.
    # DO NOT change the order.
    _COMPONENTS = (
        'core',
        'modules',
    )

    @property
    def components(self):
        """
        Returns a list of components' names where this definition is defined
        @return tuple(Component)
        """
        raise exceptions.NotImplementedError()


class DebugInfo(object):
    """DebugInfo provides some information for debugging."""

    @property
    def filepaths(self):
        """
        Returns a list of filepaths where this IDL definition comes from.
        @return tuple(FilePath)
        """
        raise exceptions.NotImplementedError()


class WithDebugInfo(object):
    """WithDebugInfo class is an interface that its inheritances can have DebugInfo."""

    @property
    def debug_info(self):
        """Returns DebugInfo."""
        raise exceptions.NotImplementedError()


class WithOwner(object):
    """WithOwner class provides information about who owns this object.
    If this object is a member, it points either of an interface,
    a namespace, a dictionary, and so on. If this object is an argument,
    it points a function like object."""

    @property
    def owner(self):
        """
        Returns the owner of this instance.
        @return object(TBD)
        """
        raise exceptions.NotImplementedError()
