# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from model import PropertyType
import code
import cpp_util
import util_cc_helper

class CCGenerator(object):
  """A .cc generator for a namespace.
  """
  def __init__(self, namespace, cpp_type_generator):
    self._cpp_type_generator = cpp_type_generator
    self._namespace = namespace
    self._target_namespace = (
        self._cpp_type_generator.GetCppNamespaceName(self._namespace))
    self._util_cc_helper = (
        util_cc_helper.UtilCCHelper(self._cpp_type_generator))

  def Generate(self):
    """Generates a code.Code object with the .cc for a single namespace.
    """
    c = code.Code()
    (c.Append(cpp_util.CHROMIUM_LICENSE)
      .Append()
      .Append(cpp_util.GENERATED_FILE_MESSAGE % self._namespace.source_file)
      .Append()
      .Append(self._util_cc_helper.GetIncludePath())
      .Append('#include "%s/%s.h"' %
          (self._namespace.source_file_dir, self._namespace.name))
    )
    includes = self._cpp_type_generator.GenerateIncludes()
    if not includes.IsEmpty():
      (c.Concat(includes)
        .Append()
      )

    (c.Append()
      .Append('using base::Value;')
      .Append('using base::DictionaryValue;')
      .Append('using base::ListValue;')
      .Append()
      .Concat(self._cpp_type_generator.GetRootNamespaceStart())
      .Concat(self._cpp_type_generator.GetNamespaceStart())
      .Append()
      .Append('//')
      .Append('// Types')
      .Append('//')
      .Append()
    )
    for type_ in self._namespace.types.values():
      (c.Concat(self._GenerateType(type_.name, type_))
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
    (c.Concat(self._cpp_type_generator.GetNamespaceEnd())
      .Concat(self._cpp_type_generator.GetRootNamespaceEnd())
      .Append()
    )
    # TODO(calamity): Events
    return c

  def _GenerateType(self, cpp_namespace, type_, serializable=True):
    """Generates the function definitions for a type.
    """
    classname = cpp_util.Classname(type_.name)
    c = code.Code()

    (c.Append('%(namespace)s::%(classname)s() {}')
      .Append('%(namespace)s::~%(classname)s() {}')
      .Append()
      .Concat(self._GenerateTypePopulate(cpp_namespace, type_))
      .Append()
    )
    if serializable:
      c.Concat(self._GenerateTypeToValue(cpp_namespace, type_))
    c.Append()
    c.Substitute({'classname': classname, 'namespace': cpp_namespace})

    return c

  def _GenerateTypePopulate(self, cpp_namespace, type_):
    """Generates the function for populating a type given a pointer to it.
    """
    classname = cpp_util.Classname(type_.name)
    c = code.Code()
    (c.Append('// static')
      .Sblock('bool %(namespace)s::Populate'
              '(const Value& value, %(name)s* out) {')
        .Append('if (!value.IsType(Value::TYPE_DICTIONARY))')
        .Append('  return false;')
        .Append('const DictionaryValue* dict = '
                'static_cast<const DictionaryValue*>(&value);')
        .Append()
    )
    for prop in type_.properties.values():
      c.Concat(self._GenerateTypePopulateProperty(prop, 'dict', 'out'))
    (c.Append('return true;')
      .Eblock('}')
    )
    c.Substitute({'namespace': cpp_namespace, 'name': classname})
    return c

  def _GenerateTypePopulateProperty(self, prop, src, dst):
    """Generate the code to populate a single property.

    src: DictionaryValue*
    dst: Type*
    """
    c = code.Code()
    dst_member = dst + '->' + prop.unix_name
    if prop.type_ == PropertyType.ARRAY:
      # util_cc_helper deals with optional and required arrays
      (c.Append('if (!%s)' %
          self._util_cc_helper.GetArray(prop, src, prop.name, dst_member))
        .Append('  return false;')
      )
    elif prop.type_ == PropertyType.CHOICES:
      value_var = prop.unix_name + '_value'
      c.Append('Value* %(value_var)s = NULL;')
      c.Append(
          'if (!%(src)s->GetWithoutPathExpansion("%(key)s", &%(value_var)s))')
      if prop.optional:
        c.Append('  return true;')
      else:
        c.Append('  return false;')
      c.Append()
      c.Concat(self._GeneratePopulateChoices(prop, value_var, dst, 'false'))
      c.Substitute({'value_var': value_var, 'key': prop.name, 'src': src})
    else:
      if prop.optional:
        if prop.type_.is_fundamental:
          (c.Sblock('{')
              .Append('%(type)s %(name)s_temp;')
              .Append('if (%s)' % self._GeneratePopulatePropertyFunctionCall(
                  prop, src, '&%s_temp' % prop.unix_name))
              .Append('  out->%(name)s.reset(new %(type)s(%(name)s_temp));')
            .Eblock('}')
          )
        else:
          raise NotImplementedError('Optional %s not implemented' % prop.type_)
      else:
        (c.Append('if (!%s)' %
            self._GeneratePopulatePropertyFunctionCall(
                prop, src, '&' + dst_member))
          .Append('  return false;')
        )
      c.Substitute({
          'name': prop.unix_name,
          'type': self._cpp_type_generator.GetType(prop)
      })
    return c

  def _GeneratePopulatePropertyFunctionCall(self, prop, src, dst):
    """Generates the function call that populates the given property.

    src: DictionaryValue*
    dst: Property* or scoped_ptr<Property>
    """
    if prop.type_.is_fundamental:
      populate_line = cpp_util.GetFundamentalValue(
          prop, src, prop.name, dst)
    elif prop.type_ in (PropertyType.REF, PropertyType.OBJECT):
      populate_line = '%(type)s::Populate(*%(src)s, %(dst)s)'
    else:
      raise NotImplementedError('%s populate is not implemented' %
          prop.type_)
    return populate_line

  def _GenerateTypeToValue(self, cpp_namespace, type_):
    """Generates a function that serializes the type into a |DictionaryValue|.
    """
    c = code.Code()
    (c.Sblock('scoped_ptr<DictionaryValue> %s::ToValue() const {' %
          cpp_namespace)
        .Append('scoped_ptr<DictionaryValue> value(new DictionaryValue());')
        .Append()
    )
    for prop in type_.properties.values():
      c.Concat(self._CreateValueFromProperty(prop, prop.unix_name, 'value'))
    (c.Append()
      .Append('return value.Pass();')
      .Eblock('}')
    )
    return c

  def _CreateValueFromProperty(self, prop, var, dst):
    """Generates code to serialize a single property in a type.

    prop: Property to create from
    var: variable with value to create from
    """
    c = code.Code()
    if prop.optional:
      c.Sblock('if (%s.get())' % var)
    if prop.type_ == PropertyType.ARRAY:
      c.Append('%s;' % self._util_cc_helper.SetArray(prop, var, prop.name, dst))
    else:
      c.Append('%s->SetWithoutPathExpansion("%s", %s);' %
          (dst, prop.name, cpp_util.CreateValueFromSingleProperty(prop, var)))
    return c

  def _GenerateFunction(self, function):
    """Generates the definitions for function structs.
    """
    classname = cpp_util.Classname(function.name)
    c = code.Code()

    # Params::Populate function
    if function.params:
      for param in function.params:
        if param.type_ == PropertyType.OBJECT:
          param_namespace = '%s::Params::%s' % (classname,
              cpp_util.Classname(param.name))
          c.Concat(
              self._GenerateType(param_namespace, param, serializable=False))
          c.Append()
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

  def _GenerateParamsCheck(self, function, var):
    """Generates a check for the correct number of arguments when creating
    Params.
    """
    c = code.Code()
    num_required = 0
    for param in function.params:
      if not param.optional:
        num_required += 1
    if num_required == len(function.params):
      c.Append('if (%(var)s.GetSize() != %(total)d)')
    elif not num_required:
      c.Append('if (%(var)s.GetSize() > %(total)s)')
    else:
      c.Append('if (%(var)s.GetSize() < %(required)d'
          ' || %(var)s.GetSize() > %(total)d)')
    c.Append('  return scoped_ptr<Params>();')
    c.Substitute({
        'var': var,
        'required': num_required,
        'total': len(function.params),
    })
    return c

  def _GenerateFunctionParamsCreate(self, function):
    """Generate function to create an instance of Params given a pointer.
    """
    classname = cpp_util.Classname(function.name)
    c = code.Code()
    (c.Append('// static')
      .Sblock('scoped_ptr<%(classname)s::Params> %(classname)s::Params::Create'
               '(const ListValue& args) {')
      .Concat(self._GenerateParamsCheck(function, 'args'))
      .Append('scoped_ptr<Params> params(new Params());')
    )
    c.Substitute({'classname': classname})

    for i, param in enumerate(function.params):
      dst = 'params->' + param.unix_name
      # Any failure will cause this function to return. If any argument is
      # incorrect or missing, those following it are not processed. Note that
      # this is still correct in the case of multiple optional arguments as an
      # optional argument at position 4 cannot exist without an argument at
      # position 3.
      if param.optional:
        failure_value = 'params.Pass()'
      else:
        failure_value = 'scoped_ptr<Params>()'
      c.Append()
      param_var = param.unix_name + '_param'
      # TODO(calamity): Return error on incorrect argument type
      if param.type_ == PropertyType.ARRAY:
        # util_cc_helper deals with optional and required arrays
        (c.Append('ListValue* %(param_var)s = NULL;')
          .Append('if (!args.GetList(%(i)d, &%(param_var)s))')
          .Append('  return %s;' % failure_value)
          .Append('if (!%s)' % self._util_cc_helper.GetArrayFromList(
              param, param_var, dst))
          .Append('  return %s;' % failure_value)
        )
        c.Substitute({'param_var': param_var, 'i': i})
      elif param.type_ == PropertyType.CHOICES:
        value_var = param.unix_name + '_value'
        (c.Append('Value* %(value_var)s = NULL;')
          .Append('if (!args.Get(%(i)s, &%(value_var)s))')
          .Append('  return %s;' % failure_value)
          .Append()
          .Concat(self._GeneratePopulateChoices(param, value_var, 'params',
              'scoped_ptr<Params>()'))
        )
        c.Substitute({'value_var': value_var, 'i': i})
      else:
        if param.optional:
          dst = dst + '.get()'
        else:
          dst = '&' + dst
        if param.type_ in (PropertyType.REF, PropertyType.OBJECT):
          (c.Append('DictionaryValue* %s = NULL;' % param_var)
            .Append('if (!args.GetDictionary(%d, &%s))' % (i, param_var))
            .Append('  return %s;' % failure_value)
          )
          if param.optional:
            c.Append('params->%s.reset(new %s());' %
                (param.unix_name, cpp_util.Classname(param.name)))
          (c.Append('if (!%(ctype)s::Populate(*%(var)s, %(dst)s))' % {
              'var': param_var, 'dst': dst,
              'ctype': self._cpp_type_generator.GetType(param)
            })
            .Append('  return %s;' % failure_value)
          )
        elif param.type_.is_fundamental:
          if param.optional:
            (c.Sblock('{')
                .Append('%(type)s %(name)s_temp;')
                .Append('if (%s)' % cpp_util.GetValueFromList(
                    param, 'args', i, '&%s_temp' % param.unix_name))
                .Append(
                    '  params->%(name)s.reset(new %(type)s(%(name)s_temp));')
              .Eblock('}')
            )
            c.Substitute({
                'name': param.unix_name,
                'type': self._cpp_type_generator.GetType(param),
            })
          else:
            (c.Append(
                'if (!%s)' % cpp_util.GetValueFromList(param, 'args', i, dst))
              .Append('  return %s;' % failure_value)
            )
        else:
          raise NotImplementedError('%s parameter is not supported' %
              param.type_)
    (c.Append()
      .Append('return params.Pass();')
      .Eblock('}')
    )

    return c

  def _GeneratePopulateChoices(self, prop, value_var, dst, failure_value):
    """Generates the code to populate a PropertyType.CHOICES parameter or
    property.

    value_var: Value*
    dst: Type* or scoped_ptr<Params>
    failure_value: the value to return on failure. Check if the property is
    optional BEFORE the code generated by this method as failure_value will be
    used to indicate a parsing error.
    """
    type_var = '%s->%s_type' % (dst, prop.unix_name)

    c = code.Code()
    c.Append('%s = %s;' %
        (type_var, self._cpp_type_generator.GetChoiceEnumNoneValue(prop)))
    c.Sblock('switch (%s->GetType()) {' % value_var)
    for choice in self._cpp_type_generator.GetExpandedChoicesInParams([prop]):
      current_choice = '%s->%s' % (dst, choice.unix_name)
      if choice.type_.is_fundamental:
        c.Sblock('case %s: {' % {
            PropertyType.STRING: 'Value::TYPE_STRING',
            PropertyType.INTEGER: 'Value::TYPE_INTEGER',
            PropertyType.BOOLEAN: 'Value::TYPE_BOOLEAN',
            PropertyType.DOUBLE: 'Value::TYPE_DOUBLE'
        }[choice.type_])

        (c.Append('%(type_var)s = %(enum_value)s;')
          .Append('%s.reset(new %s());' %
              (current_choice, self._cpp_type_generator.GetType(choice)))
          .Append('if (!%s)' %
              cpp_util.GetAsFundamentalValue(
                  choice, value_var, current_choice + '.get()'))
          .Append('  return %s;' % failure_value)
          .Append('break;')
        .Eblock('}')
        )
      elif choice.type_ == PropertyType.ARRAY:
        # util_cc_helper deals with optional and required arrays
        (c.Sblock('case Value::TYPE_LIST: {')
            .Append('%(type_var)s = %(enum_value)s;')
            .Append('if (!%s)' % self._util_cc_helper.GetArrayFromList(
                choice,
                'static_cast<ListValue*>(%s)' % value_var,
                current_choice))
            .Append('  return %s;' % failure_value)
            .Append('break;')
          .Eblock('}')
        )
      else:
        raise NotImplementedError(choice.type_)
      c.Substitute({
          'type_var': type_var,
          'enum_value': self._cpp_type_generator.GetChoiceEnumValue(
              prop, choice.type_),
      })
    if not prop.optional:
      (c.Append('default:')
        .Append('  return %s;' % failure_value)
      )
    c.Eblock('}')
    return c

  def _GenerateFunctionResultCreate(self, function):
    """Generate function to create a Result given the return value.
    """
    classname = cpp_util.Classname(function.name)
    c = code.Code()
    params = function.callback.params
    if not params:
      (c.Append('Value* %s::Result::Create() {' % classname)
        .Append('  return Value::CreateNullValue();')
        .Append('}')
      )
    else:
      # If there is a single parameter, this is straightforward. However, if
      # the callback parameter is of 'choices', this generates a Create method
      # for each choice. This works because only 1 choice can be returned at a
      # time.
      for param in self._cpp_type_generator.GetExpandedChoicesInParams(params):
        arg = cpp_util.GetConstParameterDeclaration(
            param, self._cpp_type_generator)
        c.Sblock('Value* %(classname)s::Result::Create(%(arg)s) {')
        if param.type_ == PropertyType.ARRAY:
          (c.Append('ListValue* value = new ListValue();')
            .Append('%s;' % self._util_cc_helper.SetArrayToList(
                param, param.unix_name, 'value'))
            .Append('return value;')
          )
        else:
          c.Append('return %s;' % cpp_util.CreateValueFromSingleProperty(param,
              param.unix_name))
        c.Substitute({'classname': classname, 'arg': arg})
        c.Eblock('}')

    return c
