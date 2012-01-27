# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from model import PropertyType
import code
import cpp_util
import os

class HGenerator(object):
  """A .h generator for a namespace.
  """
  def __init__(self, namespace, cpp_type_generator):
    self._cpp_type_generator = cpp_type_generator
    self._namespace = namespace
    self._target_namespace = (
        self._cpp_type_generator.GetCppNamespaceName(self._namespace))

  def Generate(self):
    """Generates a code.Code object with the .h for a single namespace.
    """
    c = code.Code()
    (c.Append(cpp_util.CHROMIUM_LICENSE)
      .Append()
      .Append(cpp_util.GENERATED_FILE_MESSAGE % self._namespace.source_file)
      .Append()
    )

    ifndef_name = self._GenerateIfndefName()
    (c.Append('#ifndef %s' % ifndef_name)
      .Append('#define %s' % ifndef_name)
      .Append('#pragma once')
      .Append()
      .Append('#include <string>')
      .Append('#include <vector>')
      .Append()
      .Append('#include "base/basictypes.h"')
      .Append('#include "base/memory/scoped_ptr.h"')
      .Append('#include "base/values.h"')
      .Append()
      .Append('using base::Value;')
      .Append('using base::DictionaryValue;')
      .Append('using base::ListValue;')
      .Append()
    )

    includes = self._cpp_type_generator.GenerateCppIncludes()
    if not includes.IsEmpty():
      (c.Concat(includes)
        .Append()
      )

    (c.Concat(self._cpp_type_generator.GetCppNamespaceStart())
      .Append()
      .Append('//')
      .Append('// Types')
      .Append('//')
      .Append()
    )
    for type_ in self._namespace.types.values():
      (c.Concat(self._GenerateType(type_))
        .Append()
      )
    (c.Append('//')
      .Append('// Functions')
      .Append('//')
      .Append()
    )
    for function in self._namespace.functions.values():
      (c.Concat(self._GenerateFunction(function))
        .Append()
      )
    (c.Append()
      .Append()
      .Concat(self._cpp_type_generator.GetCppNamespaceEnd())
      .Append()
      .Append('#endif  // %s' % ifndef_name)
      .Append()
    )
    return c

  def _GenerateType(self, type_, serializable=True):
    """Generates a struct for a type.
    """
    classname = cpp_util.CppName(type_.name)
    c = code.Code()
    if type_.description:
      c.Comment(type_.description)
    (c.Sblock('struct %(classname)s {')
        .Append('~%(classname)s();')
        .Append('%(classname)s();')
        .Append()
    )

    for prop in type_.properties.values():
      if prop.description:
        c.Comment(prop.description)
      if prop.optional:
        c.Append('scoped_ptr<%s> %s;' %
            (self._cpp_type_generator.GetType(prop, pad_for_generics=True),
            prop.name))
      else:
        c.Append('%s %s;' %
            (self._cpp_type_generator.GetType(prop), prop.name))
      c.Append()

    (c.Comment('Populates a %(classname)s object from a Value. Returns'
               ' whether |out| was successfully populated.')
      .Append('static bool Populate(const Value& value, %(classname)s* out);')
      .Append()
    )

    if serializable:
      (c.Comment('Returns a new DictionaryValue representing the'
                 ' serialized form of this %(classname)s object. Passes'
                 'ownership to caller.')
        .Append('DictionaryValue* ToValue() const;')
      )
    (c.Eblock()
    .Sblock(' private:')
      .Append('DISALLOW_COPY_AND_ASSIGN(%(classname)s);')
    .Eblock('};')
    )
    c.Substitute({'classname': classname})
    return c

  def _GenerateFunction(self, function):
    """Generates the structs for a function.
    """
    c = code.Code()
    (c.Sblock('namespace %s {' % cpp_util.CppName(function.name))
        .Concat(self._GenerateFunctionParams(function))
        .Append()
        .Concat(self._GenerateFunctionResult(function))
        .Append()
      .Eblock('};')
    )
    return c

  def _GenerateFunctionParams(self, function):
    """Generates the struct for passing parameters into a function.
    """
    c = code.Code()

    if function.params:
      c.Sblock('struct Params {')
      for param in function.params:
        if param.description:
          c.Comment(param.description)
        if param.type_ == PropertyType.OBJECT:
          c.Concat(self._GenerateType(param, serializable=False))
          c.Append()
      for param in function.params:
        c.Append('%s %s;' %
            (self._cpp_type_generator.GetType(param), param.name))

      (c.Append()
        .Append('~Params();')
        .Append()
        .Append('static scoped_ptr<Params> Create(const ListValue& args);')
      .Eblock()
      .Sblock(' private:')
        .Append('Params();')
        .Append()
        .Append('DISALLOW_COPY_AND_ASSIGN(Params);')
      )

      c.Eblock('};')

    return c

  def _GenerateFunctionResult(self, function):
    """Generates the struct for passing a function's result back.
    """
    # TODO(calamity): Handle returning an object
    c = code.Code()

    param = function.callback.param
    # TODO(calamity): Put this description comment in less stupid place
    if param.description:
      c.Comment(param.description)
    (c.Append('class Result {')
      .Sblock(' public:')
    )
    arg = ''
    # TODO(calamity): handle object
    if param:
      if param.type_ == PropertyType.REF:
        arg =  'const %(type)s& %(name)s'
      else:
        arg = 'const %(type)s %(name)s'
      arg = arg % {
          'type': self._cpp_type_generator.GetType(param),
          'name': param.name
      }
    (c.Append('static Value* Create(%s);' % arg)
    .Eblock()
    .Sblock(' private:')
      .Append('Result() {};')
      .Append('DISALLOW_COPY_AND_ASSIGN(Result);')
    .Eblock('};')
    )

    return c

  def _GenerateIfndefName(self):
    """Formats a path and filename as a #define name.

    e.g chrome/extensions/gen, file.h becomes CHROME_EXTENSIONS_GEN_FILE_H__.
    """
    return (('%s_%s_H__' %
        (self._namespace.source_file_dir, self._target_namespace))
        .upper().replace(os.sep, '_'))

