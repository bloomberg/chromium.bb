# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Builds an IdlDefinitions object from an AST (produced by blink_idl_parser)."""

import os

from idl_definitions import IdlDefinitions, IdlInterface, IdlException, IdlOperation, IdlCallbackFunction, IdlArgument, IdlAttribute, IdlConstant, IdlEnum, IdlTypedef, IdlUnionType

SPECIAL_KEYWORD_LIST = ['GETTER', 'SETTER', 'DELETER']


def build_idl_definitions_from_ast(node):
    if node is None:
        return None
    node_class = node.GetClass()
    if node_class != 'File':
        raise ValueError('Unrecognized node class: %s' % node_class)
    return file_node_to_idl_definitions(node)


def file_node_to_idl_definitions(node):
    callback_functions = {}
    enumerations = {}
    exceptions = {}
    interfaces = {}
    typedefs = {}

    # FIXME: only needed for Perl, remove later
    file_name = os.path.abspath(node.GetName())

    children = node.GetChildren()
    for child in children:
        child_class = child.GetClass()
        if child_class == 'Interface':
            interface = interface_node_to_idl_interface(child)
            interfaces[interface.name] = interface
        elif child_class == 'Exception':
            exception = exception_node_to_idl_exception(child)
            exceptions[exception.name] = exception
        elif child_class == 'Typedef':
            type_name = child.GetName()
            typedefs[type_name] = typedef_node_to_idl_typedef(child)
        elif child_class == 'Enum':
            enumeration = enum_node_to_idl_enum(child)
            enumerations[enumeration.name] = enumeration
        elif child_class == 'Callback':
            callback_function = callback_node_to_idl_callback_function(child)
            callback_functions[callback_function.name] = callback_function
        elif child_class == 'Implements':
            # Implements is handled at the interface merging step
            pass
        else:
            raise ValueError('Unrecognized node class: %s' % child_class)

    return IdlDefinitions(callback_functions=callback_functions, enumerations=enumerations, exceptions=exceptions, file_name=file_name, interfaces=interfaces, typedefs=typedefs)

# Constructors for Interface definitions and interface members


def interface_node_to_idl_interface(node):
    attributes = []
    constants = []
    constructors = None
    custom_constructors = None
    extended_attributes = None
    operations = []
    is_callback = node.GetProperty('CALLBACK') or False
    # FIXME: uppercase 'Partial' in base IDL parser
    is_partial = node.GetProperty('Partial') or False
    name = node.GetName()
    parent = None

    children = node.GetChildren()
    for child in children:
        child_class = child.GetClass()
        if child_class == 'Attribute':
            attribute = attribute_node_to_idl_attribute(child)
            # FIXME: This is a hack to support [CustomConstructor] for
            # window.HTMLImageElement. Remove the hack.
            clear_constructor_attributes(attribute.extended_attributes)
            attributes.append(attribute)
        elif child_class == 'Const':
            constants.append(constant_node_to_idl_constant(child))
        elif child_class == 'ExtAttributes':
            extended_attributes = ext_attributes_node_to_extended_attributes(child)
            constructors, custom_constructors = extended_attributes_to_constructors(extended_attributes)
            clear_constructor_attributes(extended_attributes)
        elif child_class == 'Operation':
            operations.append(operation_node_to_idl_operation(child))
        elif child_class == 'Inherit':
            parent = child.GetName()
        else:
            raise ValueError('Unrecognized node class: %s' % child_class)

    return IdlInterface(name=name, attributes=attributes, constants=constants, constructors=constructors, custom_constructors=custom_constructors, extended_attributes=extended_attributes, operations=operations, is_callback=is_callback, is_partial=is_partial, parent=parent)


def attribute_node_to_idl_attribute(node):
    data_type = None
    extended_attributes = {}
    is_nullable = False
    is_read_only = node.GetProperty('READONLY') or False
    is_static = node.GetProperty('STATIC') or False
    name = node.GetName()

    children = node.GetChildren()
    for child in children:
        child_class = child.GetClass()
        if child_class == 'Type':
            data_type = type_node_to_type(child)
            is_nullable = child.GetProperty('NULLABLE') or False
        elif child_class == 'ExtAttributes':
            extended_attributes = ext_attributes_node_to_extended_attributes(child)
        else:
            raise ValueError('Unrecognized node class: %s' % child_class)

    return IdlAttribute(data_type=data_type, extended_attributes=extended_attributes, is_nullable=is_nullable, is_read_only=is_read_only, is_static=is_static, name=name)


def constant_node_to_idl_constant(node):
    name = node.GetName()

    children = node.GetChildren()
    num_children = len(children)
    if num_children < 2 or num_children > 3:
        raise ValueError('Expected 2 or 3 children, got %s' % num_children)

    type_node = children[0]
    # ConstType is more limited than Type, so subtree is smaller and we don't
    # use the full type_node_to_type function.
    data_type = type_node_inner_to_type(type_node)

    value_node = children[1]
    value_node_class = value_node.GetClass()
    if value_node_class != 'Value':
        raise ValueError('Expected Value node, got %s' % value_node_class)
    value = value_node.GetName()

    extended_attributes = None
    if num_children == 3:
        ext_attributes_node = children[2]
        extended_attributes = ext_attributes_node_to_extended_attributes(ext_attributes_node)

    return IdlConstant(data_type=data_type, extended_attributes=extended_attributes, name=name, value=value)


def operation_node_to_idl_operation(node):
    name = node.GetName()
    # FIXME: AST should use None internally
    if name == '_unnamed_':
        name = None

    is_static = node.GetProperty('STATIC') or False
    specials = []
    property_dictionary = node.GetProperties()
    for special_keyword in SPECIAL_KEYWORD_LIST:
        if special_keyword in property_dictionary:
            specials.append(special_keyword.lower())

    extended_attributes = None
    arguments = []
    return_type = None
    children = node.GetChildren()
    for child in children:
        child_class = child.GetClass()
        if child_class == 'Arguments':
            arguments = arguments_node_to_arguments(child)
        elif child_class == 'Type':
            return_type = type_node_to_type(child)
        elif child_class == 'ExtAttributes':
            extended_attributes = ext_attributes_node_to_extended_attributes(child)
        else:
            raise ValueError('Unrecognized node class: %s' % child_class)

    return IdlOperation(name=name, data_type=return_type, extended_attributes=extended_attributes, is_static=is_static, arguments=arguments, specials=specials)


def arguments_node_to_arguments(node):
    # [Constructor] and [CustomConstructor] without arguments (the bare form)
    # have None instead of an arguments node, but have the same meaning as using
    # an empty argument list, [Constructor()], so special-case this.
    # http://www.w3.org/TR/WebIDL/#Constructor
    if node is None:
        return []
    arguments = []
    argument_node_list = node.GetChildren()
    for argument_node in argument_node_list:
        arguments.append(argument_node_to_idl_argument(argument_node))
    return arguments


def argument_node_to_idl_argument(node):
    name = node.GetName()

    data_type = None
    extended_attributes = {}
    # FIXME: Boolean values are inconsistent due to Perl compatibility.
    # Make all default to False once Perl removed.
    is_nullable = False
    is_optional = node.GetProperty('OPTIONAL')
    is_variadic = None
    children = node.GetChildren()
    for child in children:
        child_class = child.GetClass()
        if child_class == 'Type':
            data_type = type_node_to_type(child)
            is_nullable = child.GetProperty('NULLABLE')
        elif child_class == 'ExtAttributes':
            extended_attributes = ext_attributes_node_to_extended_attributes(child)
        elif child_class == 'Argument':
            child_name = child.GetName()
            if child_name != '...':
                raise ValueError('Unrecognized Argument node; expected "...", got "%s"' % child_name)
            is_variadic = child.GetProperty('ELLIPSIS') or False
        else:
            raise ValueError('Unrecognized node class: %s' % child_class)

    return IdlArgument(name=name, data_type=data_type, extended_attributes=extended_attributes, is_nullable=is_nullable, is_optional=is_optional, is_variadic=is_variadic)

# Constructors for for non-interface definitions


def callback_node_to_idl_callback_function(node):
    name = node.GetName()
    children = node.GetChildren()
    num_children = len(children)
    if num_children != 2:
        raise ValueError('Expected 2 children, got %s' % num_children)

    type_node = children[0]
    data_type = type_node_to_type(type_node)

    arguments_node = children[1]
    arguments_node_class = arguments_node.GetClass()
    if arguments_node_class != 'Arguments':
        raise ValueError('Expected Value node, got %s' % arguments_node_class)
    arguments = arguments_node_to_arguments(arguments_node)

    return IdlCallbackFunction(name=name, data_type=data_type, arguments=arguments)


def enum_node_to_idl_enum(node):
    name = node.GetName()
    values = []
    for child in node.GetChildren():
        values.append(child.GetName())
    return IdlEnum(name=name, values=values)


def exception_operation_node_to_idl_operation(node):
    # Needed to handle one case in DOMException.idl:
    # // Override in a Mozilla compatible format
    # [NotEnumerable] DOMString toString();
    # FIXME: can we remove this? replace with a stringifier?
    extended_attributes = {}
    name = node.GetName()
    children = node.GetChildren()
    if len(children) < 1 or len(children) > 2:
        raise ValueError('ExceptionOperation node with %s children, expected 1 or 2' % len(children))

    type_node = children[0]
    return_type = type_node_to_type(type_node)

    if len(children) > 1:
        ext_attributes_node = children[1]
        extended_attributes = ext_attributes_node_to_extended_attributes(ext_attributes_node)

    return IdlOperation(name=name, data_type=return_type, extended_attributes=extended_attributes)


def exception_node_to_idl_exception(node):
    # Exceptions are similar to Interfaces, but simpler
    attributes = []
    constants = []
    extended_attributes = None
    operations = []
    name = node.GetName()

    children = node.GetChildren()
    for child in children:
        child_class = child.GetClass()
        if child_class == 'Attribute':
            attribute = attribute_node_to_idl_attribute(child)
            attributes.append(attribute)
        elif child_class == 'Const':
            constants.append(constant_node_to_idl_constant(child))
        elif child_class == 'ExtAttributes':
            extended_attributes = ext_attributes_node_to_extended_attributes(child)
        elif child_class == 'ExceptionOperation':
            operations.append(exception_operation_node_to_idl_operation(child))
        else:
            raise ValueError('Unrecognized node class: %s' % child_class)

    return IdlException(name=name, attributes=attributes, constants=constants, extended_attributes=extended_attributes, operations=operations)


def typedef_node_to_idl_typedef(node):
    data_type = None
    extended_attributes = None

    children = node.GetChildren()
    for child in children:
        child_class = child.GetClass()
        if child_class == 'Type':
            data_type = type_node_to_type(child)
        elif child_class == 'ExtAttributes':
            extended_attributes = ext_attributes_node_to_extended_attributes(child)
            raise ValueError('Extended attributes in a typedef are untested!')
        else:
            raise ValueError('Unrecognized node class: %s' % child_class)

    return IdlTypedef(data_type=data_type, extended_attributes=extended_attributes)

# Extended attributes


def ext_attributes_node_to_extended_attributes(node):
    """
    Returns:
      Dictionary of {ExtAttributeName: ExtAttributeValue}.
      Value is usually a string, with three exceptions:
      Constructors: value is a list of Arguments nodes, corresponding to
      possibly signatures of the constructor.
      CustomConstructors: value is a list of Arguments nodes, corresponding to
      possibly signatures of the custom constructor.
      NamedConstructor: value is a Call node, corresponding to the single
      signature of the named constructor.
    """
    # Primarily just make a dictionary from the children.
    # The only complexity is handling various types of constructors:
    # Constructors and Custom Constructors can have duplicate entries due to
    # overloading, and thus are stored in temporary lists.
    # However, Named Constructors cannot be overloaded, and thus do not have
    # a list.
    # FIXME: Add overloading for Named Constructors and remove custom bindings
    # for HTMLImageElement
    constructors = []
    custom_constructors = []
    extended_attributes = {}

    attribute_list = node.GetChildren()
    for attribute in attribute_list:
        name = attribute.GetName()
        children = attribute.GetChildren()
        if name in ['Constructor', 'CustomConstructor', 'NamedConstructor']:
            child = None
            child_class = None
            if children:
                if len(children) > 1:
                    raise ValueError('ExtAttributes node with %s children, expected at most 1' % len(children))
                child = children[0]
                child_class = child.GetClass()
            if name == 'Constructor':
                if child_class and child_class != 'Arguments':
                    raise ValueError('Constructor only supports Arguments as child, but has child of class: %s' % child_class)
                constructors.append(child)
            elif name == 'CustomConstructor':
                if child_class and child_class != 'Arguments':
                    raise ValueError('Custom Constructor only supports Arguments as child, but has child of class: %s' % child_class)
                custom_constructors.append(child)
            else:  # name == 'NamedConstructor'
                if child_class and child_class != 'Call':
                    raise ValueError('Named Constructor only supports Call as child, but has child of class: %s' % child_class)
                extended_attributes[name] = child
        elif children:
            raise ValueError('Non-constructor ExtAttributes node with children: %s' % name)
        else:
            value = attribute.GetProperty('VALUE')
            extended_attributes[name] = value

    # Store constructors and custom constructors in special list attributes,
    # which are deleted later. Note plural in key.
    if constructors:
        extended_attributes['Constructors'] = constructors
    if custom_constructors:
        extended_attributes['CustomConstructors'] = custom_constructors

    return extended_attributes


def extended_attributes_to_constructors(extended_attributes):
    """Returns constructors and custom_constructors (lists of IdlOperations).

    Auxiliary function for interface_node_to_idl_interface.
    """
    constructors = []
    custom_constructors = []
    if 'Constructors' in extended_attributes:
        constructor_list = extended_attributes['Constructors']
        # If not overloaded, have index 0, otherwise index from 1
        overloaded_index = 0 if len(constructor_list) == 1 else 1
        for arguments_node in constructor_list:
            name = 'Constructor'
            arguments = arguments_node_to_arguments(arguments_node)
            constructor = IdlOperation(name=name, extended_attributes=extended_attributes, overloaded_index=overloaded_index, arguments=arguments)
            constructors.append(constructor)
            overloaded_index += 1

        # Prefix 'CallWith' and 'RaisesException' with 'Constructor'
        # FIXME: Change extended attributes to include prefix explicitly.
        if 'CallWith' in extended_attributes:
            extended_attributes['ConstructorCallWith'] = extended_attributes['CallWith']
            del extended_attributes['CallWith']
        if 'RaisesException' in extended_attributes:
            extended_attributes['ConstructorRaisesException'] = extended_attributes['RaisesException']
            del extended_attributes['RaisesException']

    if 'CustomConstructors' in extended_attributes:
        custom_constructor_list = extended_attributes['CustomConstructors']
        # If not overloaded, have index 0, otherwise index from 1
        overloaded_index = 0 if len(custom_constructor_list) == 1 else 1
        for arguments_node in custom_constructor_list:
            name = 'CustomConstructor'
            arguments = arguments_node_to_arguments(arguments_node)
            custom_constructor = IdlOperation(name=name, extended_attributes=extended_attributes, overloaded_index=overloaded_index, arguments=arguments)
            custom_constructors.append(custom_constructor)
            overloaded_index += 1

    if 'NamedConstructor' in extended_attributes:
        name = 'NamedConstructor'
        call_node = extended_attributes['NamedConstructor']
        extended_attributes['NamedConstructor'] = call_node.GetName()
        overloaded_index = None  # named constructors are not overloaded
        children = call_node.GetChildren()
        if len(children) != 1:
            raise ValueError('NamedConstructor node expects 1 child, got %s.' % len(children))
        arguments_node = children[0]
        arguments = arguments_node_to_arguments(arguments_node)
        named_constructor = IdlOperation(name=name, extended_attributes=extended_attributes, overloaded_index=overloaded_index, arguments=arguments)
        constructors.append(named_constructor)

    return constructors, custom_constructors


def clear_constructor_attributes(extended_attributes):
    # Deletes Constructor*s* (plural), sets Constructor (singular)
    if 'Constructors' in extended_attributes:
        del extended_attributes['Constructors']
        extended_attributes['Constructor'] = None
    if 'CustomConstructors' in extended_attributes:
        del extended_attributes['CustomConstructors']
        extended_attributes['CustomConstructor'] = None


# Types


def type_node_to_type(node):
    children = node.GetChildren()
    if len(children) < 1 or len(children) > 2:
        raise ValueError('Type node expects 1 or 2 children (type + optional array []), got %s (multi-dimensional arrays are not supported).' % len(children))

    type_node_child = children[0]
    data_type = type_node_inner_to_type(type_node_child)

    if len(children) == 2:
        array_node = children[1]
        array_node_class = array_node.GetClass()
        if array_node_class != 'Array':
            raise ValueError('Expected Array node as TypeSuffix, got %s node.' % array_node_class)
        data_type += '[]'

    return data_type


def type_node_inner_to_type(node):
    node_class = node.GetClass()
    # Note Type*r*ef, not Typedef, meaning the type is an identifier, thus
    # either a typedef shorthand (but not a Typedef declaration itself) or an
    # interface type. We do not distinguish these, and just use the type name.
    if node_class in ['PrimitiveType', 'Typeref']:
        return node.GetName()
    elif node_class == 'Any':
        return 'any'
    elif node_class == 'Sequence':
        return sequence_node_to_type(node)
    elif node_class == 'UnionType':
        return union_type_node_to_idl_union_type(node)
    raise ValueError('Unrecognized node class: %s' % node_class)


def sequence_node_to_type(node):
    children = node.GetChildren()
    if len(children) != 1:
        raise ValueError('Sequence node expects exactly 1 child, got %s' % len(children))
    sequence_child = children[0]
    sequence_child_class = sequence_child.GetClass()
    if sequence_child_class != 'Type':
        raise ValueError('Unrecognized node class: %s' % sequence_child_class)
    sequence_type = type_node_to_type(sequence_child)
    return 'sequence<%s>' % sequence_type


def union_type_node_to_idl_union_type(node):
    union_member_types = []
    for member_type_node in node.GetChildren():
        member_type = type_node_to_type(member_type_node)
        union_member_types.append(member_type)
    return IdlUnionType(union_member_types=union_member_types)
