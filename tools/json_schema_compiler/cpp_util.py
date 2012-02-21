# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilies and constants specific to Chromium C++ code.
"""

from datetime import datetime
from model import PropertyType

CHROMIUM_LICENSE = (
"""// Copyright (c) %d The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.""" % datetime.now().year
)
GENERATED_FILE_MESSAGE = """// GENERATED FROM THE API DEFINITION IN
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

def GetValueType(prop):
  """Returns the Value::Type corresponding to the model.PropertyType.
  """
  return {
      PropertyType.STRING: 'Value::TYPE_STRING',
      PropertyType.INTEGER: 'Value::TYPE_INTEGER',
      PropertyType.BOOLEAN: 'Value::TYPE_BOOLEAN',
      PropertyType.DOUBLE: 'Value::TYPE_DOUBLE',
      PropertyType.REF: 'Value::TYPE_DICTIONARY',
      PropertyType.OBJECT: 'Value::TYPE_DICTIONARY',
      PropertyType.ARRAY: 'Value::TYPE_LIST'
  }[prop.type_]


def CreateValueFromSingleProperty(prop, var):
  """Creates a Value given a single property. Use for everything except
  PropertyType.ARRAY.

  var: variable or variable*
  """
  if prop.type_ in (PropertyType.REF, PropertyType.OBJECT):
    if prop.optional:
      return '%s->ToValue().release()' % var
    else:
      return '%s.ToValue().release()' % var
  elif prop.type_.is_fundamental:
    if prop.optional:
      var = '*' + var
    return {
        PropertyType.STRING: 'Value::CreateStringValue(%s)',
        PropertyType.BOOLEAN: 'Value::CreateBooleanValue(%s)',
        PropertyType.INTEGER: 'Value::CreateIntegerValue(%s)',
        PropertyType.DOUBLE: 'Value::CreateDoubleValue(%s)',
    }[prop.type_] % var
  else:
    raise NotImplementedError('Conversion of single %s to Value not implemented'
        % repr(prop.type_))

def GetParameterDeclaration(param, type_):
  """Gets a const parameter declaration of a given model.Property and its C++
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
