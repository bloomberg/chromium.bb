# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .attribute import Attribute
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExtendedAttributes
from .constant import Constant
from .identifier_ir_map import IdentifierIRMap
from .idl_member import IdlMember
from .idl_type import IdlType
from .make_copy import make_copy
from .operation import Operation
from .reference import RefById
from .user_defined_type import UserDefinedType


class Interface(UserDefinedType, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-interfaces"""

    class IR(IdentifierIRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     is_partial,
                     is_mixin,
                     inherited=None,
                     attributes=None,
                     constants=None,
                     operations=None,
                     iterable=None,
                     maplike=None,
                     setlike=None,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     components=None,
                     debug_info=None):
            assert isinstance(is_partial, bool)
            assert isinstance(is_mixin, bool)
            assert inherited is None or isinstance(inherited, RefById)
            assert attributes is None or isinstance(attributes, (list, tuple))
            assert constants is None or isinstance(constants, (list, tuple))
            assert operations is None or isinstance(operations, (list, tuple))
            assert iterable is None or isinstance(iterable, Iterable)
            assert maplike is None or isinstance(maplike, Maplike)
            assert setlike is None or isinstance(setlike, Setlike)

            attributes = attributes or []
            constants = constants or []
            operations = operations or []
            assert all(
                isinstance(attribute, Attribute.IR)
                for attribute in attributes)
            assert all(
                isinstance(constant, Constant.IR) for constant in constants)
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

            kind = None
            if is_partial:
                if is_mixin:
                    kind = IdentifierIRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN
                else:
                    kind = IdentifierIRMap.IR.Kind.PARTIAL_INTERFACE
            else:
                if is_mixin:
                    kind = IdentifierIRMap.IR.Kind.INTERFACE_MIXIN
                else:
                    kind = IdentifierIRMap.IR.Kind.INTERFACE
            IdentifierIRMap.IR.__init__(self, identifier=identifier, kind=kind)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(
                self, component=component, components=components)
            WithDebugInfo.__init__(self, debug_info)

            self.is_partial = is_partial
            self.is_mixin = is_mixin
            self.inherited = inherited
            self.attributes = list(attributes)
            self.constants = list(constants)
            self.operations = list(operations)
            self.iterable = iterable
            self.maplike = maplike
            self.setlike = setlike

    def __init__(self, ir):
        assert isinstance(ir, Interface.IR)
        assert not ir.is_partial

        ir = make_copy(ir)
        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, ir.extended_attributes)
        WithCodeGeneratorInfo.__init__(self, ir.code_generator_info)
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info)

        self._is_mixin = ir.is_mixin
        self._inherited = ir.inherited
        self._attributes = tuple([
            Attribute(attribute_ir, owner=self)
            for attribute_ir in ir.attributes
        ])
        self._constants = tuple([
            Constant(constant_ir, owner=self) for constant_ir in ir.constants
        ])
        self._iterable = ir.iterable
        self._maplike = ir.maplike
        self._setlike = ir.setlike

    @property
    def is_mixin(self):
        """Returns True if this is a mixin interface."""
        return self._is_mixin

    @property
    def inherited_interface(self):
        """Returns the inherited interface or None."""
        return self._inherited.target_object if self._inherited else None

    @property
    def attributes(self):
        """
        Returns attributes, including [Unforgeable] attributes in ancestors.
        """
        return self._attributes

    @property
    def operation_groups(self):
        """
        Returns groups of operations, including [Unforgeable] operations in
        ancestors.  Operation groups are sorted by their identifier.

        All operations are grouped by their identifier in OperationGroup's,
        even when there exists a single operation with that identifier.
        """
        assert False, "Not implemented yet."

    @property
    def constants(self):
        """Returns constants."""
        return self._constants

    @property
    def constructors(self):
        """Returns a constructor group."""
        assert False, "Not implemented yet."

    @property
    def named_constructor(self):
        """Returns a named constructor or None."""
        assert False, "Not implemented yet."

    @property
    def exposed_interfaces(self):
        """
        Returns a tuple of interfaces that are exposed to this interface, if
        this is a global interface.  Returns None otherwise.
        """
        assert False, "Not implemented yet."

    # Special operations
    @property
    def indexed_property_handler(self):
        """
        Returns a set of handlers (getter/setter/deleter) for the indexed
        property.
        @return IndexedPropertyHandler?
        """
        # TODO: Include anonymous handlers of ancestors. https://crbug.com/695972
        assert False, "Not implemented yet."

    @property
    def named_property_handler(self):
        """
        Returns a set of handlers (getter/setter/deleter) for the named
        property.
        @return NamedPropertyHandler?
        """
        # TODO: Include anonymous handlers of ancestors. https://crbug.com/695972
        assert False, "Not implemented yet."

    @property
    def stringifier(self):
        """
        Returns stringifier if it is defined. Returns None otherwise.
        @return TBD?
        """
        assert False, "Not implemented yet."

    @property
    def iterable(self):
        """Returns an Iterable or None."""
        return self._iterable

    @property
    def maplike(self):
        """Returns a Maplike or None."""
        return self._maplike

    @property
    def setlike(self):
        """Returns a Setlike or None."""
        return self._setlike

    # UserDefinedType overrides
    @property
    def is_interface(self):
        return True


class Iterable(WithCodeGeneratorInfo, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-iterable"""

    def __init__(self,
                 key_type=None,
                 value_type=None,
                 code_generator_info=None,
                 debug_info=None):
        assert key_type is None or isinstance(key_type, IdlType)
        # iterable is declared in either form of
        #     iterable<value_type>
        #     iterable<key_type, value_type>
        # thus |value_type| can't be None.  However, we put it after |key_type|
        # to be consistent with the format of IDL.
        assert isinstance(value_type, IdlType), "value_type must be specified"

        WithCodeGeneratorInfo.__init__(self, code_generator_info)
        WithDebugInfo.__init__(self, debug_info)

        self._key_type = key_type
        self._value_type = value_type

    @property
    def key_type(self):
        """Returns the key type or None."""
        return self._key_type

    @property
    def value_type(self):
        """Returns the value type."""
        return self._value_type


class Maplike(WithCodeGeneratorInfo, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-maplike"""

    def __init__(self,
                 key_type,
                 value_type,
                 is_readonly=False,
                 code_generator_info=None,
                 debug_info=None):
        assert isinstance(key_type, IdlType)
        assert isinstance(value_type, IdlType)
        assert isinstance(is_readonly, bool)

        WithCodeGeneratorInfo.__init__(self, code_generator_info)
        WithDebugInfo.__init__(self, debug_info)

        self._key_type = key_type
        self._value_type = value_type
        self._is_readonly = is_readonly

    @property
    def key_type(self):
        """
        Returns its key type.
        @return IdlType
        """
        return self._key_type

    @property
    def value_type(self):
        """
        Returns its value type.
        @return IdlType
        """
        return self._value_type

    @property
    def is_readonly(self):
        """
        Returns True if it's readonly.
        @return bool
        """
        return self._is_readonly


class Setlike(WithCodeGeneratorInfo, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-setlike"""

    def __init__(self,
                 value_type,
                 is_readonly=False,
                 code_generator_info=None,
                 debug_info=None):
        assert isinstance(value_type, IdlType)
        assert isinstance(is_readonly, bool)

        WithCodeGeneratorInfo.__init__(self, code_generator_info)
        WithDebugInfo.__init__(self, debug_info)

        self._value_type = value_type
        self._is_readonly = is_readonly

    @property
    def value_type(self):
        """
        Returns its value type.
        @return IdlType
        """
        return self._value_type

    @property
    def is_readonly(self):
        """
        Returns True if it's readonly.
        @return bool
        """
        return self._is_readonly


class IndexedPropertyHandler(IdlMember):
    @property
    def getter(self):
        """
        Returns an Operation for indexed property getter.
        @return Operation?
        """
        assert False, "Not implemented yet."

    @property
    def setter(self):
        """
        Returns an Operation for indexed property setter.
        @return Operation?
        """
        assert False, "Not implemented yet."

    @property
    def deleter(self):
        """
        Returns an Operation for indexed property deleter.
        @return Operation?
        """
        assert False, "Not implemented yet."


class NamedPropertyHandler(IdlMember):
    @property
    def getter(self):
        """
        Returns an Operation for named property getter.
        @return Operation?
        """
        assert False, "Not implemented yet."

    @property
    def setter(self):
        """
        Returns an Operation for named property setter.
        @return Operation?
        """
        assert False, "Not implemented yet."

    @property
    def deleter(self):
        """
        Returns an Operation for named property deleter.
        @return Operation?
        """
        assert False, "Not implemented yet."
