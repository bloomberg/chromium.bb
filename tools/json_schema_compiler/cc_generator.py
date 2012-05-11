# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from code import Code
from model import PropertyType
import any_helper
import cpp_util
import model
import schema_util
import sys
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
    self._any_helper = any_helper.AnyHelper()

  def Generate(self):
    """Generates a Code object with the .cc for a single namespace.
    """
    c = Code()
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
      .Append('using base::BinaryValue;')
      .Append('using %s;' % any_helper.ANY_CLASS)
      .Append()
      .Concat(self._cpp_type_generator.GetRootNamespaceStart())
      .Concat(self._cpp_type_generator.GetNamespaceStart())
      .Append()
    )
    if self._namespace.properties:
      (c.Append('//')
        .Append('// Properties')
        .Append('//')
        .Append()
      )
      for property in self._namespace.properties.values():
        property_code = self._cpp_type_generator.GeneratePropertyValues(
            property,
            'const %(type)s %(name)s = %(value)s;',
            nodoc=True)
        if property_code:
          c.Concat(property_code).Append()
    if self._namespace.types:
      (c.Append('//')
        .Append('// Types')
        .Append('//')
        .Append()
      )
    for type_ in self._namespace.types.values():
      (c.Concat(self._GenerateType(
        schema_util.StripSchemaNamespace(type_.name), type_)).Append()
      )
    if self._namespace.functions:
      (c.Append('//')
        .Append('// Functions')
        .Append('//')
        .Append()
      )
    for function in self._namespace.functions.values():
      (c.Concat(self._GenerateFunction(
          cpp_util.Classname(function.name), function))
        .Append()
      )
    (c.Concat(self._cpp_type_generator.GetNamespaceEnd())
      .Concat(self._cpp_type_generator.GetRootNamespaceEnd())
      .Append()
    )
    # TODO(calamity): Events
    return c

  def _GenerateType(self, cpp_namespace, type_):
    """Generates the function definitions for a type.
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
      for function in type_.functions.values():
        (c.Concat(
            self._GenerateFunction(
                cpp_namespace + '::' + cpp_util.Classname(function.name),
                function))
          .Append()
        )
    elif type_.type_ == PropertyType.OBJECT:
      (c.Concat(self._GeneratePropertyFunctions(
          cpp_namespace, type_.properties.values()))
        .Sblock('%(namespace)s::%(classname)s()')
        .Concat(self._GenerateInitializersAndBody(type_))
        .Eblock('%(namespace)s::~%(classname)s() {}')
        .Append()
      )
      if type_.from_json:
        (c.Concat(self._GenerateTypePopulate(cpp_namespace, type_))
          .Append()
        )
      if type_.from_client:
        (c.Concat(self._GenerateTypeToValue(cpp_namespace, type_))
          .Append()
        )
    c.Substitute({'classname': classname, 'namespace': cpp_namespace})

    return c

  def _GenerateInitializersAndBody(self, type_):
    items = []
    for prop in type_.properties.values():
      if prop.optional:
        continue

      t = prop.type_
      if t == PropertyType.INTEGER:
        items.append('%s(0)' % prop.unix_name)
      elif t == PropertyType.DOUBLE:
        items.append('%s(0.0)' % prop.unix_name)
      elif t == PropertyType.BOOLEAN:
        items.append('%s(false)' % prop.unix_name)
      elif t == PropertyType.BINARY:
        items.append('%s(NULL)' % prop.unix_name)
      elif (t == PropertyType.ADDITIONAL_PROPERTIES or
            t == PropertyType.ANY or
            t == PropertyType.ARRAY or
            t == PropertyType.CHOICES or
            t == PropertyType.ENUM or
            t == PropertyType.OBJECT or
            t == PropertyType.REF or
            t == PropertyType.STRING):
        # TODO(miket): It would be nice to initialize CHOICES and ENUM, but we
        # don't presently have the semantics to indicate which one of a set
        # should be the default.
        continue
      else:
        sys.exit("Unhandled PropertyType: %s" % t)

    if items:
      s = ': %s' % (', '.join(items))
    else:
      s = ''
    s = s + ' {}'
    return Code().Append(s)

  def _GenerateTypePopulate(self, cpp_namespace, type_):
    """Generates the function for populating a type given a pointer to it.

    E.g for type "Foo", generates Foo::Populate()
    """
    classname = cpp_util.Classname(schema_util.StripSchemaNamespace(type_.name))
    c = Code()
    (c.Append('// static')
      .Sblock('bool %(namespace)s::Populate'
              '(const Value& value, %(name)s* out) {')
        .Append('if (!value.IsType(Value::TYPE_DICTIONARY))')
        .Append('  return false;')
    )
    if type_.properties:
      (c.Append('const DictionaryValue* dict = '
                'static_cast<const DictionaryValue*>(&value);')
        .Append()
      )
      for prop in type_.properties.values():
        c.Concat(self._InitializePropertyToDefault(prop, 'out'))
      for prop in type_.properties.values():
        if prop.type_ == PropertyType.ADDITIONAL_PROPERTIES:
          c.Append('out->additional_properties.MergeDictionary(dict);')
          # remove all keys that are actual properties
          for cur_prop in type_.properties.values():
            if prop != cur_prop:
              c.Append('out->additional_properties'
                  '.RemoveWithoutPathExpansion("%s", NULL);' % cur_prop.name)
          c.Append()
        else:
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
    c = Code()
    value_var = prop.unix_name + '_value'
    c.Append('Value* %(value_var)s = NULL;')
    if prop.optional:
      (c.Sblock(
          'if (%(src)s->GetWithoutPathExpansion("%(key)s", &%(value_var)s)) {'
        )
        .Concat(self._GeneratePopulatePropertyFromValue(
            prop, value_var, dst, 'false'))
        .Eblock('}')
      )
    else:
      (c.Append(
          'if (!%(src)s->GetWithoutPathExpansion("%(key)s", &%(value_var)s))')
        .Append('  return false;')
        .Concat(self._GeneratePopulatePropertyFromValue(
            prop, value_var, dst, 'false'))
      )
    c.Append()
    c.Substitute({'value_var': value_var, 'key': prop.name, 'src': src})
    return c

  def _GenerateTypeToValue(self, cpp_namespace, type_):
    """Generates a function that serializes the type into a |DictionaryValue|.

    E.g. for type "Foo" generates Foo::ToValue()
    """
    c = Code()
    (c.Sblock('scoped_ptr<DictionaryValue> %s::ToValue() const {' %
          cpp_namespace)
        .Append('scoped_ptr<DictionaryValue> value(new DictionaryValue());')
        .Append()
    )
    for prop in type_.properties.values():
      if prop.type_ == PropertyType.ADDITIONAL_PROPERTIES:
        c.Append('value->MergeDictionary(&%s);' % prop.unix_name)
      else:
        if prop.optional:
          if prop.type_ == PropertyType.ENUM:
            c.Sblock('if (%s != %s)' %
                (prop.unix_name,
                    self._cpp_type_generator.GetEnumNoneValue(prop)))
          else:
            c.Sblock('if (%s.get())' % prop.unix_name)
        c.Append('value->SetWithoutPathExpansion("%s", %s);' % (
            prop.name,
            self._CreateValueFromProperty(prop, 'this->' + prop.unix_name)))
        if prop.optional:
          c.Eblock();
    (c.Append()
      .Append('return value.Pass();')
      .Eblock('}')
    )
    return c

  def _GenerateFunction(self, cpp_namespace, function):
    """Generates the definitions for function structs.
    """
    c = Code()

    # Params::Populate function
    if function.params:
      c.Concat(self._GeneratePropertyFunctions(cpp_namespace + '::Params',
          function.params))
      (c.Append('%(cpp_namespace)s::Params::Params() {}')
        .Append('%(cpp_namespace)s::Params::~Params() {}')
        .Append()
        .Concat(self._GenerateFunctionParamsCreate(cpp_namespace, function))
        .Append()
      )

    # Result::Create function
    if function.callback:
      c.Concat(self._GenerateFunctionResultCreate(cpp_namespace, function))

    c.Substitute({'cpp_namespace': cpp_namespace})

    return c

  def _GenerateCreateEnumValue(self, cpp_namespace, prop):
    """Generates CreateEnumValue() that returns the |StringValue|
    representation of an enum.
    """
    c = Code()
    c.Append('// static')
    c.Sblock('scoped_ptr<Value> %(cpp_namespace)s::CreateEnumValue(%(arg)s) {')
    c.Sblock('switch (%s) {' % prop.unix_name)
    if prop.optional:
      (c.Append('case %s: {' % self._cpp_type_generator.GetEnumNoneValue(prop))
        .Append('  return scoped_ptr<Value>();')
        .Append('}')
      )
    for enum_value in prop.enum_values:
      (c.Append('case %s: {' %
          self._cpp_type_generator.GetEnumValue(prop, enum_value))
        .Append('  return scoped_ptr<Value>(Value::CreateStringValue("%s"));' %
            enum_value)
        .Append('}')
      )
    (c.Append('default: {')
      .Append('  return scoped_ptr<Value>();')
      .Append('}')
    )
    c.Eblock('}')
    c.Eblock('}')
    c.Substitute({
        'cpp_namespace': cpp_namespace,
        'arg': cpp_util.GetParameterDeclaration(
            prop, self._cpp_type_generator.GetType(prop))
    })
    return c

  def _CreateValueFromProperty(self, prop, var):
    """Creates a Value given a property. Generated code passes ownership
    to caller.

    var: variable or variable*

    E.g for std::string, generate Value::CreateStringValue(var)
    """
    if prop.type_ == PropertyType.CHOICES:
      # CHOICES conversion not implemented. If needed, write something to
      # generate a function that returns a scoped_ptr<Value> and put it in
      # _GeneratePropertyFunctions, then use it here. Look at CreateEnumValue()
      # for reference.
      raise NotImplementedError(
          'Conversion of CHOICES to Value not implemented')
    if self._IsObjectOrObjectRef(prop):
      if prop.optional:
        return '%s->ToValue().release()' % var
      else:
        return '%s.ToValue().release()' % var
    elif prop.type_ == PropertyType.ANY:
      return '%s.DeepCopy()' % self._any_helper.GetValue(prop, var)
    elif prop.type_ == PropertyType.ADDITIONAL_PROPERTIES:
      return '%s.DeepCopy()' % var
    elif prop.type_ == PropertyType.ENUM:
      return 'CreateEnumValue(%s).release()' % var
    elif self._IsArrayOrArrayRef(prop):
      return '%s.release()' % self._util_cc_helper.CreateValueFromArray(
          self._cpp_type_generator.GetReferencedProperty(prop), var,
          prop.optional)
    elif self._IsFundamentalOrFundamentalRef(prop):
      if prop.optional:
        var = '*' + var
      return {
          PropertyType.STRING: 'Value::CreateStringValue(%s)',
          PropertyType.BOOLEAN: 'Value::CreateBooleanValue(%s)',
          PropertyType.INTEGER: 'Value::CreateIntegerValue(%s)',
          PropertyType.DOUBLE: 'Value::CreateDoubleValue(%s)',
      }[prop.type_] % var
    else:
      raise NotImplementedError('Conversion of %s to Value not '
          'implemented' % repr(prop.type_))

  def _GenerateParamsCheck(self, function, var):
    """Generates a check for the correct number of arguments when creating
    Params.
    """
    c = Code()
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

  def _GenerateFunctionParamsCreate(self, cpp_namespace, function):
    """Generate function to create an instance of Params. The generated
    function takes a ListValue of arguments.

    E.g for function "Bar", generate Bar::Params::Create()
    """
    c = Code()
    (c.Append('// static')
      .Sblock('scoped_ptr<%(cpp_namespace)s::Params> '
        '%(cpp_namespace)s::Params::Create(const ListValue& args) {')
      .Concat(self._GenerateParamsCheck(function, 'args'))
      .Append('scoped_ptr<Params> params(new Params());')
    )
    c.Substitute({'cpp_namespace': cpp_namespace})

    for param in function.params:
      c.Concat(self._InitializePropertyToDefault(param, 'params'))

    for i, param in enumerate(function.params):
      # Any failure will cause this function to return. If any argument is
      # incorrect or missing, those following it are not processed. Note that
      # for optional arguments, we allow missing arguments and proceed because
      # there may be other arguments following it.
      failure_value = 'scoped_ptr<Params>()'
      c.Append()
      value_var = param.unix_name + '_value'
      (c.Append('Value* %(value_var)s = NULL;')
        .Append('if (args.Get(%(i)s, &%(value_var)s) && '
            '!%(value_var)s->IsType(Value::TYPE_NULL))')
        .Sblock('{')
        .Concat(self._GeneratePopulatePropertyFromValue(
            param, value_var, 'params', failure_value))
        .Eblock('}')
      )
      if not param.optional:
        (c.Sblock('else {')
          .Append('return %s;' % failure_value)
          .Eblock('}')
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
    c = Code()
    c.Sblock('{')

    if self._IsFundamentalOrFundamentalRef(prop):
      if prop.optional:
        (c.Append('%(ctype)s temp;')
          .Append('if (!%s)' %
              cpp_util.GetAsFundamentalValue(
                  self._cpp_type_generator.GetReferencedProperty(prop),
                  value_var,
                  '&temp'))
          .Append('  return %(failure_value)s;')
          .Append('%(dst)s->%(name)s.reset(new %(ctype)s(temp));')
        )
      else:
        (c.Append('if (!%s)' %
            cpp_util.GetAsFundamentalValue(
                self._cpp_type_generator.GetReferencedProperty(prop),
                value_var,
                '&%s->%s' % (dst, prop.unix_name)))
          .Append('  return %(failure_value)s;')
        )
    elif self._IsObjectOrObjectRef(prop):
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
    elif prop.type_ == PropertyType.ANY:
      if prop.optional:
        c.Append('%(dst)s->%(name)s.reset(new Any());')
      c.Append(self._any_helper.Init(prop, value_var, dst) + ';')
    elif self._IsArrayOrArrayRef(prop):
      # util_cc_helper deals with optional and required arrays
      (c.Append('ListValue* list = NULL;')
        .Append('if (!%(value_var)s->GetAsList(&list))')
        .Append('  return %(failure_value)s;')
        .Append('if (!%s)' % self._util_cc_helper.PopulateArrayFromList(
            self._cpp_type_generator.GetReferencedProperty(prop), 'list',
            dst + '->' + prop.unix_name, prop.optional))
        .Append('  return %(failure_value)s;')
      )
    elif prop.type_ == PropertyType.CHOICES:
      type_var = '%(dst)s->%(name)s_type'
      c.Sblock('switch (%(value_var)s->GetType()) {')
      for choice in self._cpp_type_generator.GetExpandedChoicesInParams([prop]):
        (c.Sblock('case %s: {' % cpp_util.GetValueType(
            self._cpp_type_generator.GetReferencedProperty(choice).type_))
            .Concat(self._GeneratePopulatePropertyFromValue(
                choice, value_var, dst, failure_value, check_type=False))
            .Append('%s = %s;' %
                (type_var,
                 self._cpp_type_generator.GetEnumValue(
                     prop, choice.type_.name)))
            .Append('break;')
          .Eblock('}')
        )
      (c.Append('default:')
        .Append('  return %(failure_value)s;')
      )
      c.Eblock('}')
    elif prop.type_ == PropertyType.ENUM:
      (c.Append('std::string enum_temp;')
        .Append('if (!%(value_var)s->GetAsString(&enum_temp))')
        .Append('  return %(failure_value)s;')
      )
      for i, enum_value in enumerate(prop.enum_values):
        (c.Append(
            ('if' if i == 0 else 'else if') +
            '(enum_temp == "%s")' % enum_value)
          .Append('  %s->%s = %s;' % (
            dst,
            prop.unix_name,
            self._cpp_type_generator.GetEnumValue(prop, enum_value)))
        )
      (c.Append('else')
        .Append('  return %(failure_value)s;')
      )
    elif prop.type_ == PropertyType.BINARY:
      # This is the same if the property is optional or not. We need a pointer
      # to the BinaryValue to be able to populate it, so a scoped_ptr is used
      # whether it is optional or required.
      (c.Append('if (!%(value_var)s->IsType(%(value_type)s))')
        .Append('  return %(failure_value)s;')
        .Append('%(dst)s->%(name)s.reset(')
        .Append('    static_cast<BinaryValue*>(%(value_var)s)->DeepCopy());')
      )
    else:
      raise NotImplementedError(prop.type_)
    c.Eblock('}')
    sub = {
        'value_var': value_var,
        'name': prop.unix_name,
        'dst': dst,
        'failure_value': failure_value,
    }
    if prop.type_ not in (PropertyType.CHOICES, PropertyType.ANY):
      sub['ctype'] = self._cpp_type_generator.GetType(prop)
      sub['value_type'] = cpp_util.GetValueType(self._cpp_type_generator
          .GetReferencedProperty(prop).type_)
    c.Substitute(sub)
    return c

  def _GeneratePropertyFunctions(self, param_namespace, params):
    """Generate the functions for structures generated by a property such as
    CreateEnumValue for ENUMs and Populate/ToValue for Params/Result objects.
    """
    c = Code()
    for param in params:
      if param.type_ == PropertyType.OBJECT:
        c.Concat(self._GenerateType(
            param_namespace + '::' + cpp_util.Classname(param.name),
            param))
        c.Append()
      elif param.type_ == PropertyType.CHOICES:
        c.Concat(self._GeneratePropertyFunctions(
            param_namespace, param.choices.values()))
      elif param.type_ == PropertyType.ENUM:
        c.Concat(self._GenerateCreateEnumValue(param_namespace, param))
        c.Append()
    return c

  def _GenerateFunctionResultCreate(self, cpp_namespace, function):
    """Generate function to create a Result given the return value.

    E.g for function "Bar", generate Bar::Result::Create
    """
    c = Code()
    params = function.callback.params

    if not params:
      (c.Append('Value* %s::Result::Create() {' % cpp_namespace)
        .Append('  return Value::CreateNullValue();')
        .Append('}')
      )
    else:
      expanded_params = self._cpp_type_generator.GetExpandedChoicesInParams(
          params)
      c.Concat(self._GeneratePropertyFunctions(
          cpp_namespace + '::Result', expanded_params))

      # If there is a single parameter, this is straightforward. However, if
      # the callback parameter is of 'choices', this generates a Create method
      # for each choice. This works because only 1 choice can be returned at a
      # time.
      for param in expanded_params:
        if param.type_ == PropertyType.ANY:
          # Generation of Value* Create(Value*) is redundant.
          continue
        # We treat this argument as 'required' to avoid wrapping it in a
        # scoped_ptr if it's optional.
        param_copy = param.Copy()
        param_copy.optional = False
        c.Sblock('Value* %(cpp_namespace)s::Result::Create(const %(arg)s) {')
        c.Append('return %s;' %
           self._CreateValueFromProperty(param_copy, param_copy.unix_name))
        c.Eblock('}')
        c.Substitute({
            'cpp_namespace': cpp_namespace,
            'arg': cpp_util.GetParameterDeclaration(
                param_copy, self._cpp_type_generator.GetType(param_copy))
        })

    return c

  def _InitializePropertyToDefault(self, prop, dst):
    """Initialize a model.Property to its default value inside an object.

    E.g for optional enum "state", generate dst->state = STATE_NONE;

    dst: Type*
    """
    c = Code()
    if prop.type_ in (PropertyType.ENUM, PropertyType.CHOICES):
      if prop.optional:
        prop_name = prop.unix_name
        if prop.type_ == PropertyType.CHOICES:
          prop_name = prop.unix_name + '_type'
        c.Append('%s->%s = %s;' % (
          dst,
          prop_name,
          self._cpp_type_generator.GetEnumNoneValue(prop)))
    return c

  def _IsObjectOrObjectRef(self, prop):
    """Determines if this property is an Object or is a ref to an Object.
    """
    return (self._cpp_type_generator.GetReferencedProperty(prop).type_ ==
        PropertyType.OBJECT)

  def _IsArrayOrArrayRef(self, prop):
    """Determines if this property is an Array or is a ref to an Array.
    """
    return (self._cpp_type_generator.GetReferencedProperty(prop).type_ ==
        PropertyType.ARRAY)

  def _IsFundamentalOrFundamentalRef(self, prop):
    """Determines if this property is a Fundamental type or is a ref to a
    Fundamental type.
    """
    return (self._cpp_type_generator.GetReferencedProperty(prop).type_.
        is_fundamental)
