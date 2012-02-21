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
    )
    if self._namespace.types:
      (c.Append('//')
        .Append('// Types')
        .Append('//')
        .Append()
      )
    for type_ in self._namespace.types.values():
      (c.Concat(self._GenerateType(type_.name, type_))
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
    """Generate the code to populate a single property in a type.

    src: DictionaryValue*
    dst: Type*
    """
    c = code.Code()
    value_var = prop.unix_name + '_value'
    c.Append('Value* %(value_var)s = NULL;')
    if prop.optional:
      (c.Sblock(
          'if (%(src)s->GetWithoutPathExpansion("%(key)s", &%(value_var)s)) {'
        )
        .Concat(self._GeneratePopulatePropertyFromValue(
            prop, value_var, 'out', 'false'))
        .Eblock('}')
      )
    else:
      (c.Append(
          'if (!%(src)s->GetWithoutPathExpansion("%(key)s", &%(value_var)s))')
        .Append('  return false;')
        .Concat(self._GeneratePopulatePropertyFromValue(
            prop, value_var, 'out', 'false'))
      )
    c.Append()
    c.Substitute({'value_var': value_var, 'key': prop.name, 'src': src})
    return c

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
      c.Append('%s;' % self._util_cc_helper.PopulateDictionaryFromArray(
          prop, var, prop.name, dst))
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
      c.Append('if (%(var)s.GetSize() > %(total)d)')
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
    """Generate function to create an instance of Params. The generated
    function takes a ListValue of arguments.
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
      # Any failure will cause this function to return. If any argument is
      # incorrect or missing, those following it are not processed. Note that
      # this is still correct in the case of multiple optional arguments as an
      # optional argument at position 4 cannot exist without an argument at
      # position 3.
      failure_value = 'scoped_ptr<Params>()'
      if param.optional:
        arg_missing_value = 'params.Pass()'
      else:
        arg_missing_value = failure_value
      c.Append()
      value_var = param.unix_name + '_value'
      (c.Append('Value* %(value_var)s = NULL;')
        .Append('if (!args.Get(%(i)s, &%(value_var)s) || '
            '%(value_var)s->IsType(Value::TYPE_NULL))')
        .Append('  return %s;' % arg_missing_value)
        .Concat(self._GeneratePopulatePropertyFromValue(
            param, value_var, 'params', failure_value))
      )
      c.Substitute({'value_var': value_var, 'i': i})
    (c.Append()
      .Append('return params.Pass();')
      .Eblock('}')
      .Append()
    )

    return c

  def _GeneratePopulatePropertyFromValue(
      self, prop, value_var, dst, failure_value, check_type=True):
    """Generates code to populate a model.Property given a Value*. The
    existence of data inside the Value* is assumed so checks for existence
    should be performed before the code this generates.

    prop: the property the code is populating.
    value_var: a Value* that should represent |prop|.
    dst: the object with |prop| as a member.
    failure_value: the value to return if |prop| cannot be extracted from
    |value_var|
    check_type: if true, will check if |value_var| is the correct Value::Type
    """
    c = code.Code()
    c.Sblock('{')

    if check_type:
      (c.Append('if (!%(value_var)s->IsType(%(value_type)s))')
        .Append('  return %(failure_value)s;')
      )

    if prop.type_.is_fundamental:
      if prop.optional:
        (c.Append('%(ctype)s temp;')
          .Append('if (%s)' %
              cpp_util.GetAsFundamentalValue(prop, value_var, '&temp'))
          .Append('  %(dst)s->%(name)s.reset(new %(ctype)s(temp));')
        )
      else:
        (c.Append('if (!%s)' %
            cpp_util.GetAsFundamentalValue(
                prop, value_var, '&%s->%s' % (dst, prop.unix_name)))
          .Append('return %(failure_value)s;')
        )
    elif prop.type_ in (PropertyType.OBJECT, PropertyType.REF):
      if prop.optional:
        (c.Append('DictionaryValue* dictionary = NULL;')
          .Append('if (!%(value_var)s->GetAsDictionary(&dictionary))')
          .Append('  return %(failure_value)s;')
          .Append('scoped_ptr<%(ctype)s> temp(new %(ctype)s());')
          .Append('if (!%(ctype)s::Populate(*dictionary, temp.get()))')
          .Append('  return %(failure_value)s;')
          .Append('%(dst)s->%(name)s = temp.Pass();')
        )
      else:
        (c.Append('DictionaryValue* dictionary = NULL;')
          .Append('if (!%(value_var)s->GetAsDictionary(&dictionary))')
          .Append('  return %(failure_value)s;')
          .Append(
              'if (!%(ctype)s::Populate(*dictionary, &%(dst)s->%(name)s))')
          .Append('  return %(failure_value)s;')
        )
    elif prop.type_ == PropertyType.ARRAY:
      # util_cc_helper deals with optional and required arrays
      (c.Append('ListValue* list = NULL;')
        .Append('if (!%(value_var)s->GetAsList(&list))')
        .Append('  return %s;' % failure_value)
        .Append('if (!%s)' % self._util_cc_helper.PopulateArrayFromList(
            prop, 'list', dst + '->' + prop.unix_name))
        .Append('  return %s;' % failure_value)
      )
    elif prop.type_ == PropertyType.CHOICES:
      return self._GeneratePopulateChoices(prop, value_var, dst, failure_value)
    else:
      raise NotImplementedError(prop.type_)
    c.Eblock('}')
    c.Substitute({
        'value_var': value_var,
        'name': prop.unix_name,
        'dst': dst,
        'ctype': self._cpp_type_generator.GetType(prop),
        'failure_value': failure_value,
        'value_type': cpp_util.GetValueType(prop),
    })
    return c

  def _GeneratePopulateChoices(self, prop, value_var, dst, failure_value):
    """Generates the code to populate a PropertyType.CHOICES parameter or
    property. The existence of data inside the Value* is assumed so checks for
    existence should be performed before the code this generates.

    prop: the property the code is populating..
    value_var: a Value* that should represent |prop|.
    dst: the object with |prop| as a member.
    failure_value: the value to return if |prop| cannot be extracted from
    |value_var|
    """
    type_var = '%s->%s_type' % (dst, prop.unix_name)

    c = code.Code()
    c.Append('%s = %s;' %
        (type_var, self._cpp_type_generator.GetChoiceEnumNoneValue(prop)))
    c.Sblock('switch (%s->GetType()) {' % value_var)
    for choice in self._cpp_type_generator.GetExpandedChoicesInParams([prop]):
      (c.Sblock('case %s: {' % cpp_util.GetValueType(choice))
          .Concat(self._GeneratePopulatePropertyFromValue(
              choice, value_var, dst, failure_value, check_type=False))
          .Append('%s = %s;' %
              (type_var,
               self._cpp_type_generator.GetChoiceEnumValue(
                   prop, choice.type_)))
          .Append('break;')
        .Eblock('}')
      )
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
        # We treat this argument as 'required' to avoid wrapping it in a
        # scoped_ptr if it's optional.
        param_copy = param.Copy()
        param_copy.optional = False
        c.Sblock('Value* %(classname)s::Result::Create(const %(arg)s) {')
        if param_copy.type_ == PropertyType.ARRAY:
          (c.Append('ListValue* value = new ListValue();')
            .Append('%s;' % self._util_cc_helper.PopulateListFromArray(
                param_copy, param_copy.unix_name, 'value'))
            .Append('return value;')
          )
        else:
          c.Append('return %s;' %
              cpp_util.CreateValueFromSingleProperty(param_copy,
                  param_copy.unix_name))
        c.Substitute({'classname': classname,
            'arg': cpp_util.GetParameterDeclaration(
                param_copy, self._cpp_type_generator.GetType(param_copy))
        })
        c.Eblock('}')

    return c
