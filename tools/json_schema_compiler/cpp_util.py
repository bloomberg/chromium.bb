# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilies and constants specific to Chromium C++ code.
"""

from datetime import datetime
from model import PropertyType
import os

CHROMIUM_LICENSE = (
"""// Copyright (c) %d The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.""" % datetime.now().year
)
GENERATED_FILE_MESSAGE = """// GENERATED FROM THE API DEFINITION IN
//   %s
// DO NOT EDIT.
"""
GENERATED_BUNDLE_FILE_MESSAGE = """// GENERATED FROM THE API DEFINITIONS IN
//   %s
// DO NOT EDIT.
"""

def Classname(s):
  """Translates a namespace name or function name into something more
  suited to C++.

  eg experimental.downloads -> Experimental_Downloads
  updateAll -> UpdateAll.
  """
  return '_'.join([x[0].upper() + x[1:] for x in s.split('.')])

def GetAsFundamentalValue(prop, src, dst):
  """Returns the C++ code for retrieving a fundamental type from a
  Value into a variable.

  src: Value*
  dst: Property*
  """
  return {
      PropertyType.STRING: '%s->GetAsString(%s)',
      PropertyType.BOOLEAN: '%s->GetAsBoolean(%s)',
      PropertyType.INTEGER: '%s->GetAsInteger(%s)',
      PropertyType.DOUBLE: '%s->GetAsDouble(%s)',
  }[prop.type_] % (src, dst)

def GetValueType(type_):
  """Returns the Value::Type corresponding to the model.PropertyType.
  """
  return {
      PropertyType.STRING: 'Value::TYPE_STRING',
      PropertyType.INTEGER: 'Value::TYPE_INTEGER',
      PropertyType.BOOLEAN: 'Value::TYPE_BOOLEAN',
      PropertyType.DOUBLE: 'Value::TYPE_DOUBLE',
      PropertyType.ENUM: 'Value::TYPE_STRING',
      PropertyType.OBJECT: 'Value::TYPE_DICTIONARY',
      PropertyType.ARRAY: 'Value::TYPE_LIST',
      PropertyType.BINARY: 'Value::TYPE_BINARY',
  }[type_]

def GetParameterDeclaration(param, type_):
  """Gets a parameter declaration of a given model.Property and its C++
  type.
  """
  if param.type_ in (PropertyType.REF, PropertyType.OBJECT, PropertyType.ARRAY,
      PropertyType.STRING):
    arg = '%(type)s& %(name)s'
  else:
    arg = '%(type)s %(name)s'
  return arg % {
    'type': type_,
    'name': param.unix_name,
  }

def GenerateIfndefName(path, filename):
    """Formats a path and filename as a #define name.

    e.g chrome/extensions/gen, file.h becomes CHROME_EXTENSIONS_GEN_FILE_H__.
    """
    return (('%s_%s_H__' % (path, filename))
            .upper().replace(os.sep, '_').replace('/', '_'))
