# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate template contexts of dictionaries for both v8 bindings and
implementation classes that are used by blink's core/modules.
"""

import copy
import operator
from v8_globals import includes
import v8_types
import v8_utilities


DICTIONARY_H_INCLUDES = frozenset([
    'bindings/core/v8/V8Binding.h',
    'platform/heap/Handle.h',
])

DICTIONARY_CPP_INCLUDES = frozenset([
    # FIXME: Remove this, http://crbug.com/321462
    'bindings/core/v8/Dictionary.h',
])


def setter_name_for_dictionary_member(member):
    return 'set%s' % v8_utilities.capitalize(member.name)


def has_name_for_dictionary_member(member):
    return 'has%s' % v8_utilities.capitalize(member.name)


# Context for V8 bindings

def dictionary_context(dictionary):
    includes.clear()
    includes.update(DICTIONARY_CPP_INCLUDES)
    return {
        'cpp_class': v8_utilities.cpp_name(dictionary),
        'header_includes': set(DICTIONARY_H_INCLUDES),
        'members': [member_context(member)
                    for member in sorted(dictionary.members,
                                         key=operator.attrgetter('name'))],
        'v8_class': v8_utilities.v8_class_name(dictionary),
    }


def member_context(member):
    idl_type = member.idl_type
    idl_type.add_includes_for_type()

    def idl_type_for_default_value():
        copied_type = copy.copy(idl_type)
        # IdlType for default values shouldn't be nullable. Otherwise,
        # it will generate meaningless expression like
        # 'String("default value").isNull() ? ...'.
        copied_type.is_nullable = False
        return copied_type

    def default_values():
        if not member.default_value:
            return None, None
        if member.default_value.is_null:
            return None, 'v8::Null(isolate)'
        cpp_default_value = str(member.default_value)
        v8_default_value = idl_type_for_default_value().cpp_value_to_v8_value(
            cpp_value=cpp_default_value, isolate='isolate',
            creation_context='creationContext')
        return cpp_default_value, v8_default_value

    cpp_default_value, v8_default_value = default_values()

    return {
        'cpp_default_value': cpp_default_value,
        'cpp_type': idl_type.cpp_type,
        'cpp_value_to_v8_value': idl_type.cpp_value_to_v8_value(
            cpp_value='impl->%s()' % member.name, isolate='isolate',
            creation_context='creationContext',
            extended_attributes=member.extended_attributes),
        'has_name': has_name_for_dictionary_member(member),
        'name': member.name,
        'setter_name': setter_name_for_dictionary_member(member),
        'v8_default_value': v8_default_value,
        'v8_type': v8_types.v8_type(idl_type.base_type),
    }

# FIXME: Implement context for impl class.
