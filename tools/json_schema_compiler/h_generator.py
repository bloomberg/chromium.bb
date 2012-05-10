# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from code import Code
from model import PropertyType
import cpp_util
import model
import os
import schema_util

class HGenerator(object):
  """A .h generator for a namespace.
  """
  def __init__(self, namespace, cpp_type_generator):
    self._cpp_type_generator = cpp_type_generator
    self._namespace = namespace
    self._target_namespace = (
        self._cpp_type_generator.GetCppNamespaceName(self._namespace))

  def Generate(self):
    """Generates a Code object with the .h for a single namespace.
    """
    c = Code()
    (c.Append(cpp_util.CHROMIUM_LICENSE)
      .Append()
      .Append(cpp_util.GENERATED_FILE_MESSAGE % self._namespace.source_file)
      .Append()
    )

    ifndef_name = cpp_util.GenerateIfndefName(self._namespace.source_file_dir,
                                              self._target_namespace)
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
      .Append('#include "tools/json_schema_compiler/any.h"')
      .Append()
    )

    c.Concat(self._cpp_type_generator.GetRootNamespaceStart())
    # TODO(calamity): These forward declarations should be #includes to allow
    # $ref types from other files to be used as required params. This requires
    # some detangling of windows and tabs which will currently lead to circular
    # #includes.
    forward_declarations = (
        self._cpp_type_generator.GenerateForwardDeclarations())
    if not forward_declarations.IsEmpty():
      (c.Append()
        .Concat(forward_declarations)
        .Append()
      )

    c.Concat(self._cpp_type_generator.GetNamespaceStart())
    c.Append()
    if self._namespace.properties:
      (c.Append('//')
        .Append('// Properties')
        .Append('//')
        .Append()
      )
      for property in self._namespace.properties.values():
        property_code = self._cpp_type_generator.GeneratePropertyValues(
            property,
            'extern const %(type)s %(name)s;')
        if property_code:
          c.Concat(property_code).Append()
    if self._namespace.types:
      (c.Append('//')
        .Append('// Types')
        .Append('//')
        .Append()
      )
      for type_ in self._FieldDependencyOrder():
        (c.Concat(self._GenerateType(type_))
          .Append()
        )
    if self._namespace.functions:
      (c.Append('//')
        .Append('// Functions')
        .Append('//')
        .Append()
      )
      for function in self._namespace.functions.values():
        (c.Concat(self._GenerateFunction(function))
          .Append()
        )
    (c.Concat(self._cpp_type_generator.GetNamespaceEnd())
      .Concat(self._cpp_type_generator.GetRootNamespaceEnd())
      .Append()
      .Append('#endif  // %s' % ifndef_name)
      .Append()
    )
    return c

  def _FieldDependencyOrder(self):
    """Generates the list of types in the current namespace in an order in which
    depended-upon types appear before types which depend on them.
    """
    dependency_order = []

    def ExpandType(path, type_):
      if type_ in path:
        raise ValueError("Illegal circular dependency via cycle " +
                         ", ".join(map(lambda x: x.name, path + [type_])))
      for prop in type_.properties.values():
        if not prop.optional and prop.type_ == PropertyType.REF:
          ExpandType(path + [type_], self._namespace.types[prop.ref_type])
      if not type_ in dependency_order:
        dependency_order.append(type_)

    for type_ in self._namespace.types.values():
      ExpandType([], type_)
    return dependency_order

  def _GenerateEnumDeclaration(self, enum_name, prop, values):
    """Generate the declaration of a C++ enum for the given property and
    values.
    """
    c = Code()
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
    c = Code()
    # Generate the enums needed for any fields with "choices"
    for prop in props:
      if prop.type_ == PropertyType.CHOICES:
        enum_name = self._cpp_type_generator.GetChoicesEnumType(prop)
        c.Append('%s %s_type;' % (enum_name, prop.unix_name))
        c.Append()
    for prop in self._cpp_type_generator.GetExpandedChoicesInParams(props):
      if prop.description:
        c.Comment(prop.description)
      c.Append('%s %s;' % (
          self._cpp_type_generator.GetType(prop, wrap_optional=True),
          prop.unix_name))
      c.Append()
    return c

  def _GenerateType(self, type_):
    """Generates a struct for a type.
    """
    classname = cpp_util.Classname(schema_util.StripSchemaNamespace(type_.name))
    c = Code()

    if type_.functions:
      # Types with functions are not instantiable in C++ because they are
      # handled in pure Javascript and hence have no properties or
      # additionalProperties.
      if type_.properties:
        raise NotImplementedError('\n'.join(model.GetModelHierarchy(type_)) +
            '\nCannot generate both functions and properties on a type')
      c.Sblock('namespace %(classname)s {')
      for function in type_.functions.values():
        (c.Concat(self._GenerateFunction(function))
          .Append()
        )
      c.Eblock('}')
    elif type_.type_ == PropertyType.ARRAY:
      if type_.description:
        c.Comment(type_.description)
      c.Append('typedef std::vector<%(item_type)s> %(classname)s;')
      c.Substitute({'classname': classname, 'item_type':
          self._cpp_type_generator.GetType(type_.item_type,
                                           wrap_optional=True)})
    elif type_.type_ == PropertyType.STRING:
      if type_.description:
        c.Comment(type_.description)
      c.Append('typedef std::string %(classname)s;')
      c.Substitute({'classname': classname})
    else:
      if type_.description:
        c.Comment(type_.description)
      (c.Sblock('struct %(classname)s {')
          .Append('~%(classname)s();')
          .Append('%(classname)s();')
          .Append()
          .Concat(self._GeneratePropertyStructures(type_.properties.values()))
          .Concat(self._GenerateFields(type_.properties.values()))
      )
      if type_.from_json:
        (c.Comment('Populates a %s object from a Value. Returns'
                   ' whether |out| was successfully populated.' % classname)
          .Append(
              'static bool Populate(const Value& value, %(classname)s* out);')
          .Append()
        )

      if type_.from_client:
        (c.Comment('Returns a new DictionaryValue representing the'
                   ' serialized form of this %s object. Passes '
                   'ownership to caller.' % classname)
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
    c = Code()
    (c.Sblock('namespace %s {' % cpp_util.Classname(function.name))
        .Concat(self._GenerateFunctionParams(function))
        .Append()
    )
    if function.callback:
      (c.Concat(self._GenerateFunctionResult(function))
        .Append()
      )
    c.Eblock('};')

    return c

  def _GenerateFunctionParams(self, function):
    """Generates the struct for passing parameters into a function.
    """
    c = Code()

    if function.params:
      (c.Sblock('struct Params {')
          .Concat(self._GeneratePropertyStructures(function.params))
          .Concat(self._GenerateFields(function.params))
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

  def _GeneratePropertyStructures(self, props):
    """Generate the structures required by a property such as OBJECT classes
    and enums.
    """
    c = Code()
    for prop in props:
      if prop.type_ == PropertyType.OBJECT:
        c.Concat(self._GenerateType(prop))
        c.Append()
      elif prop.type_ == PropertyType.CHOICES:
        c.Concat(self._GenerateEnumDeclaration(
            self._cpp_type_generator.GetChoicesEnumType(prop),
            prop,
            [choice.type_.name for choice in prop.choices.values()]))
        c.Concat(self._GeneratePropertyStructures(prop.choices.values()))
      elif prop.type_ == PropertyType.ENUM:
        enum_name = self._cpp_type_generator.GetType(prop)
        c.Concat(self._GenerateEnumDeclaration(
            enum_name,
            prop,
            prop.enum_values))
        c.Append('static scoped_ptr<Value> CreateEnumValue(%s %s);' %
            (enum_name, prop.unix_name))
    return c

  def _GenerateFunctionResult(self, function):
    """Generates functions for passing a function's result back.
    """
    c = Code()

    c.Sblock('namespace Result {')
    params = function.callback.params
    if not params:
      c.Append('Value* Create();')
    else:
      c.Concat(self._GeneratePropertyStructures(params))

      # If there is a single parameter, this is straightforward. However, if
      # the callback parameter is of 'choices', this generates a Create method
      # for each choice. This works because only 1 choice can be returned at a
      # time.
      for param in self._cpp_type_generator.GetExpandedChoicesInParams(params):
        if param.description:
          c.Comment(param.description)
        if param.type_ == PropertyType.ANY:
          c.Comment("Value* Result::Create(Value*) not generated "
                    "because it's redundant.")
          continue
        c.Append('Value* Create(const %s);' % cpp_util.GetParameterDeclaration(
            param, self._cpp_type_generator.GetType(param)))
    c.Eblock('};')

    return c
