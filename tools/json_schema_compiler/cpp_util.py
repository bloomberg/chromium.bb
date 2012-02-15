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

def CreateFundamentalValue(prop, var):
  """Returns the C++ code for creating a value of the given property type
  using the given variable.

  var: Fundamental or Fundamental*
  """
  if prop.optional:
    var = '*' + var
  return {
      PropertyType.STRING: 'Value::CreateStringValue(%s)',
      PropertyType.BOOLEAN: 'Value::CreateBooleanValue(%s)',
      PropertyType.INTEGER: 'Value::CreateIntegerValue(%s)',
      PropertyType.DOUBLE: 'Value::CreateDoubleValue(%s)',
  }[prop.type_] % var

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

def GetFundamentalValue(prop, src, name, dst):
  """Returns the C++ code for retrieving a fundamental type from a
  DictionaryValue into a variable.

  src: DictionaryValue*
  name: key
  dst: Property*
  """
  return {
      PropertyType.STRING: '%s->GetString("%s", %s)',
      PropertyType.BOOLEAN: '%s->GetBoolean("%s", %s)',
      PropertyType.INTEGER: '%s->GetInteger("%s", %s)',
      PropertyType.DOUBLE: '%s->GetDouble("%s", %s)',
  }[prop.type_] % (src, name, dst)

def CreateValueFromSingleProperty(prop, var):
  """Creates a Value given a single property. Use for everything except
  PropertyType.ARRAY.

  var: raw value
  """
  if prop.type_ == PropertyType.REF or prop.type_ == PropertyType.OBJECT:
    if prop.optional:
      return '%s->ToValue().release()' % var
    else:
      return '%s.ToValue().release()' % var
  elif prop.type_.is_fundamental:
    return CreateFundamentalValue(prop, var)
  else:
    raise NotImplementedError('Conversion of single %s to Value not implemented'
        % repr(prop.type_))

def GetValueFromList(prop, src, index, dst):
  """Returns the C++ code for retrieving a fundamental type from a
  DictionaryValue into a variable.

  src: ListValue&
  index: int
  dst: Property*
  """
  return {
      PropertyType.REF: '%s.GetDictionary(%d, %s)',
      PropertyType.STRING: '%s.GetString(%d, %s)',
      PropertyType.BOOLEAN: '%s.GetBoolean(%d, %s)',
      PropertyType.INTEGER: '%s.GetInteger(%d, %s)',
      PropertyType.DOUBLE: '%s.GetDouble(%d, %s)',
  }[prop.type_] % (src, index, dst)

def GetConstParameterDeclaration(param, type_generator):
  if param.type_ == PropertyType.REF:
    arg = 'const %(type)s& %(name)s'
  else:
    arg = 'const %(type)s %(name)s'
  return arg % {
    'type': type_generator.GetType(param, wrap_optional=True),
    'name': param.unix_name,
  }
