# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilies for the processing of schema python structures.
"""

import json_parse

def CapitalizeFirstLetter(value):
  return value[0].capitalize() + value[1:]

def GetNamespace(ref_type):
  if '.' in ref_type:
    return ref_type[:ref_type.rindex('.')]

def StripSchemaNamespace(s):
  last_dot = s.rfind('.')
  if not last_dot == -1:
    return s[last_dot + 1:]
  return s

def JsFunctionNameToClassName(namespace_name, function_name):
  """Transform a fully qualified function name like foo.bar.baz into FooBarBaz

  Also strips any leading 'Experimental' prefix."""
  parts = []
  full_name = namespace_name + "." + function_name
  for part in full_name.split("."):
    parts.append(CapitalizeFirstLetter(part))
  if parts[0] == "Experimental":
    del parts[0]
  class_name = "".join(parts)
  return class_name
