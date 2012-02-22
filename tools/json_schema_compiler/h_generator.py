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
      .Append('#include "base/memory/linked_ptr.h"')
      .Append('#include "base/memory/scoped_ptr.h"')
      .Append('#include "base/values.h"')
      .Append()
    )

    c.Concat(self._cpp_type_generator.GetRootNamespaceStart())
    forward_declarations = (
        self._cpp_type_generator.GenerateForwardDeclarations())
    if not forward_declarations.IsEmpty():
      (c.Append()
        .Concat(forward_declarations)
        .Append()
      )

    (c.Concat(self._cpp_type_generator.GetNamespaceStart())
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
      .Concat(self._cpp_type_generator.GetNamespaceEnd())
      .Concat(self._cpp_type_generator.GetRootNamespaceEnd())
      .Append()
      .Append('#endif  // %s' % ifndef_name)
      .Append()
    )
    return c

  def _GenerateEnumDeclaration(self, enum_name, prop, values):
    c = code.Code()
    c.Sblock('enum %s {' % enum_name)
    if prop.optional:
      c.Append(self._cpp_type_generator.GetEnumNoneValue(prop) + ',')
    for value in values:
      c.Append(self._cpp_type_generator.GetEnumValue(prop, value) + ',')
    (c.Eblock('};')
      .Append()
    )
    return c

  def _GenerateFields(self, props):
    """Generates the field declarations when declaring a type.
    """
    c = code.Code()
    # Generate the enums needed for any fields with "choices"
    for prop in props:
      if prop.type_ == PropertyType.CHOICES:
        enum_name = self._cpp_type_generator.GetChoicesEnumType(prop)
        c.Concat(self._GenerateEnumDeclaration(
            enum_name,
            prop,
            [choice.type_.name for choice in prop.choices.values()]))
        c.Append('%s %s_type;' % (enum_name, prop.unix_name))
      elif prop.type_ == PropertyType.ENUM:
        c.Concat(self._GenerateEnumDeclaration(
            self._cpp_type_generator.GetType(prop),
            prop,
            prop.enum_values))
    for prop in self._cpp_type_generator.GetExpandedChoicesInParams(props):
      if prop.description:
        c.Comment(prop.description)
      c.Append('%s %s;' % (
          self._cpp_type_generator.GetType(prop, wrap_optional=True),
          prop.unix_name))
      c.Append()
    return c

  def _GenerateType(self, type_, serializable=True):
    """Generates a struct for a type.
    """
    classname = cpp_util.Classname(type_.name)
    c = code.Code()
    if type_.description:
      c.Comment(type_.description)
    (c.Sblock('struct %(classname)s {')
        .Append('~%(classname)s();')
        .Append('%(classname)s();')
        .Append()
        .Concat(self._GenerateFields(type_.properties.values()))
        .Comment('Populates a %(classname)s object from a Value. Returns'
                 ' whether |out| was successfully populated.')
        .Append('static bool Populate(const Value& value, %(classname)s* out);')
        .Append()
    )

    if serializable:
      (c.Comment('Returns a new DictionaryValue representing the'
                 ' serialized form of this %(classname)s object. Passes '
                 'ownership to caller.')
        .Append('scoped_ptr<DictionaryValue> ToValue() const;')
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
    (c.Sblock('namespace %s {' % cpp_util.Classname(function.name))
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
        if param.type_ == PropertyType.OBJECT:
          c.Concat(self._GenerateType(param, serializable=False))
          c.Append()
      (c.Concat(self._GenerateFields(function.params))
        .Append('~Params();')
        .Append()
        .Append('static scoped_ptr<Params> Create(const ListValue& args);')
      .Eblock()
      .Sblock(' private:')
        .Append('Params();')
        .Append()
        .Append('DISALLOW_COPY_AND_ASSIGN(Params);')
      .Eblock('};')
      )

    return c

  def _GenerateFunctionResult(self, function):
    """Generates functions for passing a function's result back.
    """
    c = code.Code()

    c.Sblock('namespace Result {')
    params = function.callback.params
    if not params:
      c.Append('Value* Create();')
    else:
      # If there is a single parameter, this is straightforward. However, if
      # the callback parameter is of 'choices', this generates a Create method
      # for each choice. This works because only 1 choice can be returned at a
      # time.
      for param in self._cpp_type_generator.GetExpandedChoicesInParams(params):
        if param.description:
          c.Comment(param.description)
        if param.type_ == PropertyType.OBJECT:
          raise NotImplementedError('OBJECT return type not supported')
        c.Append('Value* Create(const %s);' % cpp_util.GetParameterDeclaration(
            param, self._cpp_type_generator.GetType(param)))
    c.Eblock('};')

    return c

  def _GenerateIfndefName(self):
    """Formats a path and filename as a #define name.

    e.g chrome/extensions/gen, file.h becomes CHROME_EXTENSIONS_GEN_FILE_H__.
    """
    return (('%s_%s_H__' %
        (self._namespace.source_file_dir, self._target_namespace))
        .upper().replace(os.sep, '_'))

