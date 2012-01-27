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


def CppName(s):
  """Translates a namespace name or function name into something more
  suited to C++.

  eg experimental.downloads -> Experimental_Downloads
  updateAll -> UpdateAll.
  """
  return '_'.join([x[0].upper() + x[1:] for x in s.split('.')])

def CreateFundamentalValue(prop, var):
  """Returns the C++ code for creating a value of the given property type
  using the given variable.
  """
  return {
      PropertyType.STRING: 'Value::CreateStringValue(%s)',
      PropertyType.BOOLEAN: 'Value::CreateBooleanValue(%s)',
      PropertyType.INTEGER: 'Value::CreateIntegerValue(%s)',
      PropertyType.DOUBLE: 'Value::CreateDoubleValue(%s)',
  }[prop.type_] % var


def GetFundamentalValue(prop, var):
  """Returns the C++ code for retrieving a fundamental type from a Value
  into a variable.
  """
  return {
      PropertyType.STRING: 'GetAsString(%s)',
      PropertyType.BOOLEAN: 'GetAsBoolean(%s)',
      PropertyType.INTEGER: 'GetAsInteger(%s)',
      PropertyType.DOUBLE: 'GetAsDouble(%s)',
  }[prop.type_] % var
