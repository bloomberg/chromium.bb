# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

API_UTIL_NAMESPACE = 'json_schema_compiler::util'

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
        'namespace': API_UTIL_NAMESPACE,
        'name': name,
        'src': src,
        'dst': dst,
    }

    sub['type'] = self._type_manager.GetType(prop)
    if array_prop.optional:
      val = ('%(namespace)s::PopulateOptionalArrayFromDictionary'
          '(*%(src)s, "%(name)s", &%(dst)s)')
    else:
      val = ('%(namespace)s::PopulateArrayFromDictionary'
          '(*%(src)s, "%(name)s", &%(dst)s)')

    return val % sub

  def PopulateArrayFromList(self, array_prop, src, dst):
    """Generates code to get an array from src into dst.

    src: ListValue*
    dst: std::vector or scoped_ptr<std::vector>
    """
    prop = array_prop.item_type
    sub = {
        'namespace': API_UTIL_NAMESPACE,
        'src': src,
        'dst': dst,
        'type': self._type_manager.GetType(prop),
    }

    if array_prop.optional:
      val = '%(namespace)s::PopulateOptionalArrayFromList(*%(src)s, &%(dst)s)'
    else:
      val = '%(namespace)s::PopulateArrayFromList(*%(src)s, &%(dst)s)'

    return val % sub

  def PopulateDictionaryFromArray(self, array_prop, src, name, dst):
    """Generates code to set dst.name to the array at src

    src: std::vector or scoped_ptr<std::vector>
    dst: scoped_ptr<DictionaryValue>
    """
    prop = array_prop.item_type
    sub = {
        'namespace': API_UTIL_NAMESPACE,
        'src': src,
        'name': name,
        'dst': dst,
        'type': self._type_manager.GetType(prop),
    }

    if array_prop.optional:
      val = ('%(namespace)s::PopulateDictionaryFromOptionalArray'
          '(%(src)s, "%(name)s", %(dst)s.get())')
    else:
      val = ('%(namespace)s::PopulateDictionaryFromArray'
          '(%(src)s, "%(name)s", %(dst)s.get())')

    return val % sub

  def PopulateListFromArray(self, array_prop, src, dst):
    """Generates code to set dst to the array at src

    src: std::vector or scoped_ptr<std::vector>
    dst: ListValue*
    """
    prop = array_prop.item_type
    sub = {
        'namespace': API_UTIL_NAMESPACE,
        'src': src,
        'dst': dst,
        'type': self._type_manager.GetType(prop),
    }

    if array_prop.optional:
      val = '%(namespace)s::PopulateListFromOptionalArray(%(src)s, %(dst)s)'
    else:
      val = '%(namespace)s::PopulateListFromArray(%(src)s, %(dst)s)'

    return val % sub

  def GetIncludePath(self):
    return '#include "tools/json_schema_compiler/util.h"'
