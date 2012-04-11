# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ANY_NAMESPACE = 'json_schema_compiler::any'
ANY_CLASS = ANY_NAMESPACE + '::Any'

class AnyHelper(object):
  """A util class that generates code that uses
  tools/json_schema_compiler/any.cc.
  """
  def Init(self, any_prop, src, dst):
    """Initialize |dst|.|any_prop| to |src|.

    src: Value*
    dst: Type*
    """
    if any_prop.optional:
      return '%s->%s->Init(*%s)' % (dst, any_prop.unix_name, src)
    else:
      return '%s->%s.Init(*%s)' % (dst, any_prop.unix_name, src)

  def GetValue(self, any_prop, var):
    """Get |var| as a const Value&.

    var: Any* or Any
    """
    if any_prop.optional:
      return '%s->value()' % var
    else:
      return '%s.value()' % var

