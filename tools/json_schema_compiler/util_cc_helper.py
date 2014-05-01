# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from model import PropertyType


_API_UTIL_NAMESPACE = 'json_schema_compiler::util'


class UtilCCHelper(object):
  """A util class that generates code that uses
  tools/json_schema_compiler/util.cc.
  """
  def __init__(self, type_manager):
    self._type_manager = type_manager

  def PopulateArrayFromDictionary(self, array_prop, src, name, dst):
    """Generates code to get an array from a src.name into dst.

    src: DictionaryValue*
    dst: std::vector or scoped_ptr<std::vector>
    """
    prop = array_prop.item_type
    sub = {
        'namespace': _API_UTIL_NAMESPACE,
        'name': name,
        'src': src,
        'dst': dst,
    }

    sub['type'] = self._type_manager.GetCppType(prop),
    if array_prop.optional:
      val = ('%(namespace)s::PopulateOptionalArrayFromDictionary'
          '(*%(src)s, "%(name)s", &%(dst)s)')
    else:
      val = ('%(namespace)s::PopulateArrayFromDictionary'
          '(*%(src)s, "%(name)s", &%(dst)s)')

    return val % sub

  def PopulateArrayFromList(self, src, dst, optional):
    """Generates code to get an array from src into dst.

    src: ListValue*
    dst: std::vector or scoped_ptr<std::vector>
    """
    if optional:
      val = '%(namespace)s::PopulateOptionalArrayFromList(*%(src)s, &%(dst)s)'
    else:
      val = '%(namespace)s::PopulateArrayFromList(*%(src)s, &%(dst)s)'
    return val % {
      'namespace': _API_UTIL_NAMESPACE,
      'src': src,
      'dst': dst
    }

  def CreateValueFromArray(self, cpp_namespace, type_, src, optional):
    """Generates code to create a scoped_pt<Value> from the array at src.

    |cpp_namespace| The namespace which contains |type_|. This is needed for
        enum conversions, where the ToString method is on the containing
        namespace.
    |type_| The type of the values being converted. This is needed for enum
        conversions, to know whether to use the Enum form of conversion.
    |src| The variable to convert, either a vector or scoped_ptr<vector>.
    |optional| Whether |type_| was optional. Optional types are pointers so
        must be treated differently.
    """
    if type_.item_type.property_type == PropertyType.ENUM:
      # Enums are treated specially because C++ templating thinks that they're
      # ints, but really they're strings.
      if optional:
        name = 'CreateValueFromOptionalEnumArray<%s>' % cpp_namespace
      else:
        name = 'CreateValueFromEnumArray<%s>' % cpp_namespace
    else:
      if optional:
        name = 'CreateValueFromOptionalArray'
      else:
        name = 'CreateValueFromArray'
    return '%s::%s(%s)' % (_API_UTIL_NAMESPACE, name, src)

  def GetIncludePath(self):
    return '#include "tools/json_schema_compiler/util.h"'

  def GetValueTypeString(self, value, is_ptr=False):
    call = '.GetType()'
    if is_ptr:
      call = '->GetType()'
    return 'json_schema_compiler::util::ValueTypeToString(%s%s)' % (value, call)
