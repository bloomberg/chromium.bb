# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from model import PropertyType
import code
import cpp_util

class CCGenerator(object):
  """A .cc generator for a namespace.
  """
  def __init__(self, namespace, cpp_type_generator):
    self._cpp_type_generator = cpp_type_generator
    self._namespace = namespace
    self._target_namespace = (
        self._cpp_type_generator.GetCppNamespaceName(self._namespace))

  def Generate(self):
    """Generates a code.Code object with the .cc for a single namespace.
    """
    c = code.Code()
    (c.Append(cpp_util.CHROMIUM_LICENSE)
      .Append()
      .Append(cpp_util.GENERATED_FILE_MESSAGE % self._namespace.source_file)
      .Append()
      .Append('#include "tools/json_schema_compiler/util.h"')
      .Append('#include "%s/%s.h"' %
          (self._namespace.source_file_dir, self._target_namespace))
      .Append()
      .Concat(self._cpp_type_generator.GetCppNamespaceStart())
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
    (c.Concat(self._cpp_type_generator.GetCppNamespaceEnd())
      .Append()
    )
    # TODO(calamity): Events
    return c

  def _GenerateType(self, type_):
    """Generates the function definitions for a type.
    """
    c = code.Code()

    (c.Append('%(classname)s::%(classname)s() {}')
      .Append('%(classname)s::~%(classname)s() {}')
      .Append()
    )
    c.Substitute({'classname': type_.name})

    c.Concat(self._GenerateTypePopulate(type_))
    c.Append()
    # TODO(calamity): deal with non-serializable
    c.Concat(self._GenerateTypeTovalue(type_))
    c.Append()

    return c

  def _GenerateTypePopulate(self, type_):
    """Generates the function for populating a type given a pointer to it.
    """
    c = code.Code()
    (c.Append('// static')
      .Sblock('bool %(name)s::Populate(const Value& value, %(name)s* out) {')
        .Append('if (!value.IsType(Value::TYPE_DICTIONARY))')
        .Append('  return false;')
        .Append('const DictionaryValue* dict = '
                'static_cast<const DictionaryValue*>(&value);')
        .Append()
    )
    c.Substitute({'name': type_.name})

    # TODO(calamity): this handle single PropertyType.REF properties.
    # add ALL the types
    for prop in type_.properties.values():
      sub = {'name': prop.name}
      if prop.type_ == PropertyType.ARRAY:
        if prop.item_type.type_ == PropertyType.REF:
          if prop.optional:
            (c.Append('if (!json_schema_compiler::util::'
                      'GetOptionalTypes<%(type)s>(*dict,')
              .Append('    "%(name)s", &out->%(name)s))')
              .Append('  return false;')
            )
          else:
            (c.Append('if (!json_schema_compiler::util::'
                      'GetTypes<%(type)s>(*dict,')
              .Append('    "%(name)s", &out->%(name)s))')
              .Append('  return false;')
            )
          sub['type'] = self._cpp_type_generator.GetType(prop.item_type,
              pad_for_generics=True)
        elif prop.item_type.type_ == PropertyType.STRING:
          if prop.optional:
            (c.Append('if (!json_schema_compiler::util::GetOptionalStrings'
                      '(*dict, "%(name)s", &out->%(name)s))')
              .Append('  return false;')
            )
          else:
            (c.Append('if (!json_schema_compiler::util::GetStrings'
                      '(*dict, "%(name)s", &out->%(name)s))')
              .Append('  return false;')
            )
        else:
          raise NotImplementedError(prop.item_type.type_)
      elif prop.type_.is_fundamental:
        c.Append('if (!dict->%s)' %
            cpp_util.GetFundamentalValue(prop, '&out->%s' % prop.name))
        c.Append('  return false;')
      else:
        raise NotImplementedError(prop.type_)
      c.Substitute(sub)
    (c.Append('return true;')
    .Eblock('}')
    )
    return c

  def _GenerateTypeTovalue(self, type_):
    """Generates a function that serializes the type into a |DictionaryValue|.
    """
    c = code.Code()
    (c.Sblock('DictionaryValue* %s::ToValue() const {' % type_.name)
      .Append('DictionaryValue* value = new DictionaryValue();')
      .Append()
    )
    name = type_.name.lower()
    for prop in type_.properties.values():
      prop_name = name + '_' + prop.name if name else prop.name
      this_var =  prop.name
      c.Concat(self._CreateValueFromProperty(prop_name, prop, this_var))
    (c.Append()
      .Append('return value;')
      .Eblock('}')
    )
    return c

  # TODO(calamity): object and choices prop types
  def _CreateValueFromProperty(self, name, prop, var):
    """Generates code to serialize a single property in a type.
    """
    c = code.Code()
    if prop.type_.is_fundamental:
      c.Append('Value* %s_value = %s;' %
          (name, cpp_util.CreateFundamentalValue(prop, var)))
    elif prop.type_ == PropertyType.ARRAY:
      if prop.item_type.type_ == PropertyType.STRING:
        if prop.optional:
          c.Append('json_schema_compiler::util::'
                   'SetOptionalStrings(%s, "%s", value);' % (var, prop.name))
        else:
          c.Append('json_schema_compiler::util::'
                   'SetStrings(%s, "%s", value);' % (var, prop.name))
      else:
        item_name = name + '_single'
        (c.Append('ListValue* %(name)s_value = new ListValue();')
          .Append('for (%(it_type)s::iterator it = %(var)s->begin();')
          .Sblock('    it != %(var)s->end(); ++it) {')
          .Concat(self._CreateValueFromProperty(item_name, prop.item_type,
                  '*it'))
          .Append('%(name)s_value->Append(%(prop_val)s_value);')
          .Eblock('}')
        )
        c.Substitute(
            {'it_type': self._cpp_type_generator.GetType(prop),
             'name': name, 'var': var, 'prop_val': item_name})
    elif prop.type_ == PropertyType.REF:
      c.Append('Value* %s_value = %s.ToValue();' % (name, var))
    else:
      raise NotImplementedError
    return c

  def _GenerateFunction(self, function):
    """Generates the definitions for function structs.
    """
    classname = cpp_util.CppName(function.name)
    c = code.Code()

    # Params::Populate function
    if function.params:
      (c.Append('%(name)s::Params::Params() {}')
        .Append('%(name)s::Params::~Params() {}')
        .Append()
        .Concat(self._GenerateFunctionParamsCreate(function))
        .Append()
      )

    # Result::Create function
    c.Concat(self._GenerateFunctionResultCreate(function))

    c.Substitute({'name': classname})

    return c

  def _GenerateFunctionParamsCreate(self, function):
    """Generate function to create an instance of Params given a pointer.
    """
    classname = cpp_util.CppName(function.name)
    c = code.Code()
    (c.Append('// static')
      .Sblock('scoped_ptr<%(classname)s::Params> %(classname)s::Params::Create'
               '(const ListValue& args) {')
      .Append('if (args.GetSize() != %d)' % len(function.params))
      .Append('  return scoped_ptr<Params>();')
      .Append()
      .Append('scoped_ptr<Params> params(new Params());')
    )
    c.Substitute({'classname': classname})

    # TODO(calamity): generalize, needs to move to function to do populates for
    # wider variety of args
    for i, param in enumerate(function.params):
      sub = {'name': param.name, 'pos': i}
      c.Append()
      # TODO(calamity): Make valid for not just objects
      c.Append('DictionaryValue* %(name)s_param = NULL;')
      c.Append('if (!args.GetDictionary(%(pos)d, &%(name)s_param))')
      c.Append('  return scoped_ptr<Params>();')
      if param.type_ == PropertyType.REF:
        c.Append('if (!%(ctype)s::Populate(*%(name)s_param, '
                 '&params->%(name)s))')
        c.Append('  return scoped_ptr<Params>();')
        sub['ctype'] = self._cpp_type_generator.GetType(param)
      elif param.type_.is_fundamental:
        raise NotImplementedError('Fundamental types are unimplemented')
      elif param.type_ == PropertyType.OBJECT:
        c.Append('if (!%(ctype)s::Populate(*%(name)s_param, '
                 '&params->%(name)s))')
        c.Append('  return scoped_ptr<Params>();')
        sub['ctype'] = self._cpp_type_generator.GetType(param)
      elif param.type_ == PropertyType.CHOICES:
        raise NotImplementedError('Choices is unimplemented')
      else:
        raise NotImplementedError(param.type_)
      c.Substitute(sub)
    c.Append()
    c.Append('return params.Pass();')
    c.Eblock('}')

    return c

  def _GenerateFunctionResultCreate(self, function):
    """Generate function to create a Result given the return value.
    """
    classname = cpp_util.CppName(function.name)
    c = code.Code()
    c.Append('// static')
    param = function.callback.param
    arg = ''
    if param:
      if param.type_ == PropertyType.REF:
        arg = 'const %(type)s& %(name)s'
      else:
        arg = 'const %(type)s %(name)s'
      arg = arg % {
          'type': self._cpp_type_generator.GetType(param),
          'name': param.name
      }
    c.Sblock('Value* %(classname)s::Result::Create(%(arg)s) {')
    sub = {'classname': classname, 'arg': arg}
    # TODO(calamity): Choices
    if not param:
      c.Append('return Value::CreateNullValue();')
    else:
      sub['argname'] = param.name
      if param.type_.is_fundamental:
        c.Append('return %s;' %
            cpp_util.CreateFundamentalValue(param, param.name))
      elif param.type_ == PropertyType.REF:
        c.Append('return %(argname)s.ToValue();')
      elif param.type_ == PropertyType.OBJECT:
        raise NotImplementedError('Objects not implemented')
      elif param.type_ == PropertyType.ARRAY:
        raise NotImplementedError('Arrays not implemented')
      else:
        raise NotImplementedError(param.type_)
    c.Substitute(sub)
    c.Eblock('}')
    return c
