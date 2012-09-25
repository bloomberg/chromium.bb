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
          (self._namespace.source_file_dir, self._namespace.unix_name))
    )
    includes = self._cpp_type_generator.GenerateIncludes()
    if not includes.IsEmpty():
      (c.Concat(includes)
        .Append()
      )

    (c.Append()
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
    if self._namespace.events:
      (c.Append('//')
        .Append('// Events')
        .Append('//')
        .Append()
      )
    for event in self._namespace.events.values():
      (c.Concat(self._GenerateCreateCallbackArguments(
          cpp_util.Classname(event.name), event, generate_to_json=True))
        .Append()
      )
    (c.Concat(self._cpp_type_generator.GetNamespaceEnd())
      .Concat(self._cpp_type_generator.GetRootNamespaceEnd())
      .Append()
    )
    return c

  def _GenerateType(self, cpp_namespace, type_):
    """Generates the function definitions for a type.
    """
    classname = cpp_util.Classname(schema_util.StripSchemaNamespace(type_.name))
    c = Code()

    if type_.functions:
      for function in type_.functions.values():
        (c.Concat(
            self._GenerateFunction(
                cpp_namespace + '::' + cpp_util.Classname(function.name),
                function))
          .Append())
    elif type_.type_ == PropertyType.OBJECT:
      (c.Concat(self._GeneratePropertyFunctions(
          cpp_namespace, type_.properties.values()))
        .Sblock('%(namespace)s::%(classname)s()')
        .Concat(self._GenerateInitializersAndBody(type_))
        .Eblock('%(namespace)s::~%(classname)s() {}')
        .Append())
      if type_.from_json:
        (c.Concat(self._GenerateTypePopulate(cpp_namespace, type_))
          .Append())
      if type_.from_client:
        (c.Concat(self._GenerateTypeToValue(cpp_namespace, type_))
          .Append())
    elif self._cpp_type_generator.IsEnumOrEnumRef(type_):
      (c.Concat(self._GenerateCreateEnumTypeValue(cpp_namespace, type_))
        .Append()
        .Concat(self._GenerateEnumFromString(cpp_namespace, type_))
        .Append()
        .Concat(self._GenerateEnumToString(cpp_namespace, type_))
        .Append())
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
            t == PropertyType.FUNCTION or
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
              '(const base::Value& value, %(name)s* out) {')
        .Append('if (!value.IsType(base::Value::TYPE_DICTIONARY))')
        .Append('  return false;')
    )
    if type_.properties:
      (c.Append('const base::DictionaryValue* dict = '
                'static_cast<const base::DictionaryValue*>(&value);')
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

    src: base::DictionaryValue*
    dst: Type*
    """
    c = Code()
    value_var = prop.unix_name + '_value'
    c.Append('const base::Value* %(value_var)s = NULL;')
    if prop.optional:
      (c.Sblock(
          'if (%(src)s->GetWithoutPathExpansion("%(key)s", &%(value_var)s)) {')
        .Concat(self._GeneratePopulatePropertyFromValue(
            prop, value_var, dst, 'false')))
      if self._cpp_type_generator.IsEnumOrEnumRef(prop):
        (c.Append('} else {')
          .Append('%%(dst)s->%%(name)s = %s;' %
              self._cpp_type_generator.GetEnumNoneValue(prop)))
      c.Eblock('}')
    else:
      (c.Append(
          'if (!%(src)s->GetWithoutPathExpansion("%(key)s", &%(value_var)s))')
        .Append('  return false;')
        .Concat(self._GeneratePopulatePropertyFromValue(
            prop, value_var, dst, 'false'))
      )
    c.Append()
    c.Substitute({
      'value_var': value_var,
      'key': prop.name,
      'src': src,
      'dst': dst,
      'name': prop.unix_name
    })
    return c

  def _GenerateTypeToValue(self, cpp_namespace, type_):
    """Generates a function that serializes the type into a
    |base::DictionaryValue|.

    E.g. for type "Foo" generates Foo::ToValue()
    """
    c = Code()
    (c.Sblock('scoped_ptr<base::DictionaryValue> %s::ToValue() const {' %
          cpp_namespace)
        .Append('scoped_ptr<base::DictionaryValue> value('
                'new base::DictionaryValue());')
        .Append()
    )
    for prop in type_.properties.values():
      if prop.type_ == PropertyType.ADDITIONAL_PROPERTIES:
        c.Append('value->MergeDictionary(&%s);' % prop.unix_name)
      else:
        if prop.optional:
          if self._cpp_type_generator.IsEnumOrEnumRef(prop):
            c.Sblock('if (%s != %s) {' %
                (prop.unix_name,
                 self._cpp_type_generator.GetEnumNoneValue(prop)))
          elif prop.type_ == PropertyType.CHOICES:
            c.Sblock('if (%s_type != %s) {' %
                (prop.unix_name,
                 self._cpp_type_generator.GetEnumNoneValue(prop)))
          else:
            c.Sblock('if (%s.get()) {' % prop.unix_name)

        if prop.type_ == prop.compiled_type:
          c.Append('value->SetWithoutPathExpansion("%s", %s);' % (
              prop.name,
              self._CreateValueFromProperty(prop, 'this->' + prop.unix_name)))
        else:
          conversion_src = 'this->' + prop.unix_name
          if prop.optional:
            conversion_src = '*' + conversion_src
          (c.Append('%s %s;' % (self._cpp_type_generator.GetType(prop),
                                prop.unix_name))
            .Append(cpp_util.GenerateCompiledTypeToTypeConversion(
                self._cpp_type_generator.GetReferencedProperty(prop),
                conversion_src,
                prop.unix_name) + ';')
            .Append('value->SetWithoutPathExpansion("%s", %s);' % (
                prop.unix_name,
                self._CreateValueFromProperty(prop, prop.unix_name)))
          )
        if prop.optional:
          c.Eblock('}');
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

    # Results::Create function
    if function.callback:
      c.Concat(self._GenerateCreateCallbackArguments(
          "%s::Results" % cpp_namespace, function.callback))

    c.Substitute({'cpp_namespace': cpp_namespace})

    return c

  def _CreateValueFromProperty(self, prop, var):
    """Creates a base::Value given a property. Generated code passes ownership
    to caller.

    var: variable or variable*

    E.g for std::string, generate base::Value::CreateStringValue(var)
    """
    if prop.type_ == PropertyType.CHOICES:
      return 'Get%sChoiceValue().release()' % cpp_util.Classname(prop.name)
    elif self._IsObjectOrObjectRef(prop):
      if prop.optional:
        return '%s->ToValue().release()' % var
      else:
        return '%s.ToValue().release()' % var
    elif prop.type_ == PropertyType.ANY:
      return '%s.DeepCopy()' % self._any_helper.GetValue(prop, var)
    elif prop.type_ == PropertyType.ADDITIONAL_PROPERTIES:
      return '%s.DeepCopy()' % var
    elif prop.type_ == PropertyType.FUNCTION:
      if prop.optional:
        vardot = var + '->'
      else:
        vardot = var + '.'
      return '%sDeepCopy()' % vardot
    elif self._cpp_type_generator.IsEnumOrEnumRef(prop):
      return 'base::Value::CreateStringValue(ToString(%s))' % var
    elif prop.type_ == PropertyType.BINARY:
      if prop.optional:
        vardot = var + '->'
      else:
        vardot = var + '.'
      return ('base::BinaryValue::CreateWithCopiedBuffer(%sdata(), %ssize())' %
              (vardot, vardot))
    elif self._IsArrayOrArrayRef(prop):
      return '%s.release()' % self._util_cc_helper.CreateValueFromArray(
          self._cpp_type_generator.GetReferencedProperty(prop), var,
          prop.optional)
    elif self._IsFundamentalOrFundamentalRef(prop):
      # If prop.type != prop.compiled_type, then no asterisk is necessary
      # because the target is a local variable and not a dereferenced scoped
      # pointer. The asterisk is instead prepended to conversion_src around line
      # 273.
      if prop.optional and prop.type_ == prop.compiled_type:
        var = '*' + var
      prop = self._cpp_type_generator.GetReferencedProperty(prop);
      return {
          PropertyType.STRING: 'base::Value::CreateStringValue(%s)',
          PropertyType.BOOLEAN: 'base::Value::CreateBooleanValue(%s)',
          PropertyType.INTEGER: 'base::Value::CreateIntegerValue(%s)',
          PropertyType.DOUBLE: 'base::Value::CreateDoubleValue(%s)',
      }[prop.type_] % var
    else:
      raise NotImplementedError('Conversion of %s to base::Value not '
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
    function takes a base::ListValue of arguments.

    E.g for function "Bar", generate Bar::Params::Create()
    """
    c = Code()
    (c.Append('// static')
      .Sblock('scoped_ptr<%(cpp_namespace)s::Params> '
        '%(cpp_namespace)s::Params::Create(const base::ListValue& args) {')
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
      (c.Append('const base::Value* %(value_var)s = NULL;')
        .Append('if (args.Get(%(i)s, &%(value_var)s) &&\n'
                '    !%(value_var)s->IsType(base::Value::TYPE_NULL))')
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
    """Generates code to populate a model.Property given a base::Value*. The
    existence of data inside the base::Value* is assumed so checks for existence
    should be performed before the code this generates.

    prop: the property the code is populating.
    value_var: a base::Value* that should represent |prop|.
    dst: the object with |prop| as a member.
    failure_value: the value to return if |prop| cannot be extracted from
    |value_var|
    check_type: if true, will check if |value_var| is the correct
    base::Value::Type
    """
    c = Code()
    c.Sblock('{')

    if self._IsFundamentalOrFundamentalRef(prop):
      self._GenerateFundamentalOrFundamentalRefPopulate(c, prop, value_var, dst)
    elif self._IsObjectOrObjectRef(prop):
      self._GenerateObjectOrObjectRefPopulate(c, prop)
    elif prop.type_ == PropertyType.FUNCTION:
      self._GenerateFunctionPopulate(c, prop)
    elif prop.type_ == PropertyType.ANY:
      self._GenerateAnyPopulate(c, prop, value_var, dst)
    elif self._IsArrayOrArrayRef(prop):
      self._GenerateArrayOrArrayRefPopulate(c, prop, dst)
    elif prop.type_ == PropertyType.CHOICES:
      self._GenerateChoicePopulate(c, prop, value_var, dst, failure_value)
    elif self._cpp_type_generator.IsEnumOrEnumRef(prop):
      self._GenerateEnumPopulate(c, prop, value_var)
    elif prop.type_ == PropertyType.BINARY:
      self._GenerateBinaryPopulate(c, prop)
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
      sub['compiled_ctype'] = self._cpp_type_generator.GetCompiledType(prop)
      sub['value_type'] = cpp_util.GetValueType(self._cpp_type_generator
          .GetReferencedProperty(prop).type_)
    c.Substitute(sub)
    return c

  def _GenerateFundamentalOrFundamentalRefPopulate(self,
                                                   c,
                                                   prop,
                                                   value_var,
                                                   dst):
    if prop.optional:
      (c.Append('%(ctype)s temp;')
        .Append('if (!%s)' %
            cpp_util.GetAsFundamentalValue(
                self._cpp_type_generator.GetReferencedProperty(prop),
                value_var,
                '&temp'))
        .Append('  return %(failure_value)s;')
      )
      if prop.type_ != prop.compiled_type:
        (c.Append('%(compiled_ctype)s temp2;')
          .Append('if (!%s)' %
               cpp_util.GenerateTypeToCompiledTypeConversion(
                   self._cpp_type_generator.GetReferencedProperty(prop),
                   'temp',
                   'temp2'))
          .Append('  return %(failure_value)s;')
          .Append('%(dst)s->%(name)s.reset(new %(compiled_ctype)s(temp2));')
        )
      else:
        c.Append('%(dst)s->%(name)s.reset(new %(ctype)s(temp));')

    else:
      if prop.type_ == prop.compiled_type:
        assignment_target = '&%s->%s' % (dst, prop.unix_name)
      else:
        c.Append('%(ctype)s temp;')
        assignment_target = '&temp'
      (c.Append('if (!%s)' %
          cpp_util.GetAsFundamentalValue(
              self._cpp_type_generator.GetReferencedProperty(prop),
              value_var,
              assignment_target))
        .Append('  return %(failure_value)s;')
      )
      if prop.type_ != prop.compiled_type:
        (c.Append('if (!%s)' %
            cpp_util.GenerateTypeToCompiledTypeConversion(
                self._cpp_type_generator.GetReferencedProperty(prop),
                'temp',
                '%s->%s' % (dst, prop.unix_name)))
          .Append('  return %(failure_value)s;')
        )

  def _GenerateObjectOrObjectRefPopulate(self, c, prop):
    if prop.optional:
      (c.Append('const base::DictionaryValue* dictionary = NULL;')
        .Append('if (!%(value_var)s->GetAsDictionary(&dictionary))')
        .Append('  return %(failure_value)s;')
        .Append('scoped_ptr<%(ctype)s> temp(new %(ctype)s());')
        .Append('if (!%(ctype)s::Populate(*dictionary, temp.get()))')
        .Append('  return %(failure_value)s;')
        .Append('%(dst)s->%(name)s = temp.Pass();')
      )
    else:
      (c.Append('const base::DictionaryValue* dictionary = NULL;')
        .Append('if (!%(value_var)s->GetAsDictionary(&dictionary))')
        .Append('  return %(failure_value)s;')
        .Append(
            'if (!%(ctype)s::Populate(*dictionary, &%(dst)s->%(name)s))')
        .Append('  return %(failure_value)s;')
      )

  def _GenerateFunctionPopulate(self, c, prop):
    if prop.optional:
      c.Append('%(dst)s->%(name)s.reset(new base::DictionaryValue());')

  def _GenerateAnyPopulate(self, c, prop, value_var, dst):
    if prop.optional:
      c.Append('%(dst)s->%(name)s.reset(new ' + any_helper.ANY_CLASS + '());')
    c.Append(self._any_helper.Init(prop, value_var, dst) + ';')

  def _GenerateArrayOrArrayRefPopulate(self, c, prop, dst):
    # util_cc_helper deals with optional and required arrays
    (c.Append('const base::ListValue* list = NULL;')
      .Append('if (!%(value_var)s->GetAsList(&list))')
      .Append('  return %(failure_value)s;'))
    if prop.item_type.type_ == PropertyType.ENUM:
      self._GenerateListValueToEnumArrayConversion(c, prop)
    else:
      (c.Append('if (!%s)' % self._util_cc_helper.PopulateArrayFromList(
            self._cpp_type_generator.GetReferencedProperty(prop), 'list',
            dst + '->' + prop.unix_name, prop.optional))
        .Append('  return %(failure_value)s;')
      )

  def _GenerateChoicePopulate(self, c, prop, value_var, dst, failure_value):
    type_var = '%(dst)s->%(name)s_type'
    c.Sblock('switch (%(value_var)s->GetType()) {')
    for choice in self._cpp_type_generator.ExpandParams([prop]):
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

  def _GenerateEnumPopulate(self, c, prop, value_var):
    c.Sblock('{')
    self._GenerateStringToEnumConversion(c, prop, value_var, 'enum_temp')
    c.Append('%(dst)s->%(name)s = enum_temp;')
    c.Eblock('}')

  def _GenerateBinaryPopulate(self, c, prop):
    (c.Append('if (!%(value_var)s->IsType(%(value_type)s))')
      .Append('  return %(failure_value)s;')
      .Append('const base::BinaryValue* binary_value =')
      .Append('    static_cast<const base::BinaryValue*>(%(value_var)s);')
     )
    if prop.optional:
      (c.Append('%(dst)s->%(name)s.reset(')
        .Append('    new std::string(binary_value->GetBuffer(),')
        .Append('                    binary_value->GetSize()));')
       )
    else:
      (c.Append('%(dst)s->%(name)s.assign(binary_value->GetBuffer(),')
        .Append('                         binary_value->GetSize());')
       )

  def _GenerateListValueToEnumArrayConversion(self, c, prop):
      """Appends code that converts a ListValue of string contstants to
      an array of enums in dst.
      Leaves dst, name, and failure_value unsubstituted.

      c: the Code object that is being appended to.
      prop: the property that the code is populating.
      """
      accessor = '.'
      if prop.optional:
        c.Append('%(dst)s->%(name)s.reset(new std::vector<' + (
          self._cpp_type_generator.GetType(prop.item_type) + '>);'))
        accessor = '->'
      c.Sblock('for (ListValue::const_iterator it = list->begin(); '
                'it != list->end(); ++it) {')
      self._GenerateStringToEnumConversion(
          c, prop.item_type, '(*it)', 'enum_temp')
      c.Append('%(dst)s->%(name)s' + accessor + 'push_back(enum_temp);')
      c.Eblock('}')

  def _GenerateStringToEnumConversion(self, c, prop, value_var, enum_temp):
    """Appends code that converts a string to an enum.
    Leaves failure_value unsubstituted.

    c: the code that is appended to.
    prop: the property that the code is populating.
    value_var: the string value that is being converted.
    enum_temp: the name used to store the temporary enum value.
    """
    (c.Append('std::string enum_as_string;')
      .Append('if (!%s->GetAsString(&enum_as_string))' % value_var)
      .Append('  return %(failure_value)s;')
      .Append('%(type)s %(enum)s = From%(type)sString(enum_as_string);' % {
        'type': self._cpp_type_generator.GetCompiledType(prop),
        'enum': enum_temp
      })
      .Append('if (%s == %s)' %
        (enum_temp, self._cpp_type_generator.GetEnumNoneValue(prop)))
     .Append('  return %(failure_value)s;'))

  def _GeneratePropertyFunctions(self, param_namespace, params):
    """Generate the functions for structures generated by a property such as
    CreateEnumValue for ENUMs and Populate/ToValue for Params/Results objects.
    """
    c = Code()
    for param in params:
      if param.type_ == PropertyType.OBJECT:
        c.Concat(self._GenerateType(
            param_namespace + '::' + cpp_util.Classname(param.name),
            param))
        c.Append()
      elif param.type_ == PropertyType.ARRAY:
        c.Concat(self._GeneratePropertyFunctions(
            param_namespace, [param.item_type]))
      elif param.type_ == PropertyType.CHOICES:
        c.Concat(self._GeneratePropertyFunctions(
            param_namespace, param.choices.values()))
        if param.from_client:
          c.Concat(self._GenerateGetChoiceValue(param_namespace, param))
      elif param.type_ == PropertyType.ENUM:
        (c.Concat(self._GenerateCreateEnumValue(param_namespace, param))
          .Append()
          .Concat(self._GenerateEnumFromString(param_namespace,
                                               param,
                                               use_namespace=True))
          .Append()
          .Concat(self._GenerateEnumToString(param_namespace,
                                             param,
                                             use_namespace=True))
          .Append())
    return c

  def _GenerateGetChoiceValue(self, cpp_namespace, prop):
    """Generates Get<Type>ChoiceValue() that returns a scoped_ptr<base::Value>
    representing the choice value.
    """
    c = Code()
    (c.Sblock('scoped_ptr<base::Value> '
              '%(cpp_namespace)s::Get%(choice)sChoiceValue() const {')
      .Sblock('switch (%s_type) {' % prop.unix_name)
      .Concat(self._GenerateReturnCase(
          self._cpp_type_generator.GetEnumNoneValue(prop),
          'scoped_ptr<base::Value>()')))
    for choice in self._cpp_type_generator.ExpandParams([prop]):
      c.Concat(self._GenerateReturnCase(
          self._cpp_type_generator.GetEnumValue(prop, choice.type_.name),
          'make_scoped_ptr<base::Value>(%s)' %
              self._CreateValueFromProperty(choice, choice.unix_name)))
    (c.Eblock('}')
      .Append('return scoped_ptr<base::Value>();')
      .Eblock('}')
      .Append()
      .Substitute({
        'cpp_namespace': cpp_namespace,
        'choice': cpp_util.Classname(prop.name)
      })
    )
    return c

  def _GenerateCreateEnumTypeValue(self, cpp_namespace, prop):
    """Generates CreateEnumValue() that returns the base::StringValue
    representation of an enum type.
    """
    c = Code()
    classname = cpp_util.Classname(schema_util.StripSchemaNamespace(prop.name))
    (c.Sblock('scoped_ptr<base::Value> CreateEnumValue(%s %s) {' %
        (classname, classname.lower()))
      .Append('std::string enum_temp = ToString(%s);' % classname.lower())
      .Append('if (enum_temp.empty())')
      .Append('  return scoped_ptr<base::Value>();')
      .Append('return scoped_ptr<base::Value>('
              'base::Value::CreateStringValue(enum_temp));')
      .Eblock('}'))
    return c

  def _GenerateEnumToString(self, cpp_namespace, prop, use_namespace=False):
    """Generates ToString() which gets the string representation of an enum.
    """
    c = Code()
    classname = cpp_util.Classname(schema_util.StripSchemaNamespace(prop.name))
    if use_namespace:
      namespace = '%s::' % cpp_namespace
    else:
      namespace = ''

    (c.Append('// static')
      .Sblock('std::string %(namespace)sToString(%(class)s enum_param) {'))
    enum_prop = self._cpp_type_generator.GetReferencedProperty(prop)
    c.Sblock('switch (enum_param) {')
    for enum_value in enum_prop.enum_values:
      c.Concat(self._GenerateReturnCase(
          self._cpp_type_generator.GetEnumValue(prop, enum_value),
          '"%s"' % enum_value))
    (c.Append('case %s:' % self._cpp_type_generator.GetEnumNoneValue(prop))
      .Append('  return "";')
      .Eblock('}')
      .Append('return "";')
      .Eblock('}')
      .Substitute({
        'namespace': namespace,
        'class': classname
      }))
    return c

  def _GenerateEnumFromString(self, cpp_namespace, prop, use_namespace=False):
    """Generates FromClassNameString() which gets an enum from its string
    representation.
    """
    c = Code()
    classname = cpp_util.Classname(schema_util.StripSchemaNamespace(prop.name))
    if use_namespace:
      namespace = '%s::' % cpp_namespace
    else:
      namespace = ''

    (c.Append('// static')
      .Sblock('%(namespace)s%(class)s'
              ' %(namespace)sFrom%(class)sString('
              'const std::string& enum_string) {'))
    enum_prop = self._cpp_type_generator.GetReferencedProperty(prop)
    for i, enum_value in enumerate(
          self._cpp_type_generator.GetReferencedProperty(prop).enum_values):
      # This is broken up into all ifs with no else ifs because we get
      # "fatal error C1061: compiler limit : blocks nested too deeply"
      # on Windows.
      (c.Append('if (enum_string == "%s")' % enum_value)
        .Append('  return %s;' %
            self._cpp_type_generator.GetEnumValue(prop, enum_value)))
    (c.Append('return %s;' %
        self._cpp_type_generator.GetEnumNoneValue(prop))
      .Eblock('}')
      .Substitute({
        'namespace': namespace,
        'class': classname
      }))
    return c

  # TODO(chebert): This is basically the same as GenerateCreateEnumTypeValue().
  # The plan is to phase out the old-style enums, and make all enums into REF
  # types.
  def _GenerateCreateEnumValue(self, cpp_namespace, prop):
    """Generates CreateEnumValue() that returns the base::StringValue
    representation of an enum.
    """
    c = Code()
    (c.Append('// static')
      .Sblock('scoped_ptr<base::Value> %(cpp_namespace)s::CreateEnumValue('
             '%(arg)s) {')
      .Append('std::string enum_temp = ToString(%s);' % prop.unix_name)
      .Append('if (enum_temp.empty())')
      .Append('  return scoped_ptr<base::Value>();')
      .Append('return scoped_ptr<base::Value>('
              'base::Value::CreateStringValue(enum_temp));')
      .Eblock('}')
      .Substitute({
        'cpp_namespace': cpp_namespace,
        'arg': cpp_util.GetParameterDeclaration(
            prop, self._cpp_type_generator.GetType(prop))
      }))
    return c

  def _GenerateReturnCase(self, case_value, return_value):
    """Generates a single return case for a switch block.
    """
    c = Code()
    (c.Append('case %s:' % case_value)
      .Append('  return %s;' % return_value)
    )
    return c

  def _GenerateCreateCallbackArguments(self,
                                       function_scope,
                                       callback,
                                       generate_to_json=False):
    """Generate all functions to create Value parameters for a callback.

    E.g for function "Bar", generate Bar::Results::Create
    E.g for event "Baz", generate Baz::Create

    function_scope: the function scope path, e.g. Foo::Bar for the function
    Foo::Bar::Baz().
    callback: the Function object we are creating callback arguments for.
    generate_to_json: Generate a ToJson method.
    """
    c = Code()
    params = callback.params
    expanded_params = self._cpp_type_generator.ExpandParams(params)
    c.Concat(self._GeneratePropertyFunctions(function_scope, expanded_params))

    param_lists = self._cpp_type_generator.GetAllPossibleParameterLists(params)
    for param_list in param_lists:
      (c.Sblock('scoped_ptr<base::ListValue> %(function_scope)s::'
                'Create(%(declaration_list)s) {')
        .Append('scoped_ptr<base::ListValue> create_results('
                'new base::ListValue());')
      )
      declaration_list = []
      for param in param_list:
        # We treat this argument as 'required' to avoid wrapping it in a
        # scoped_ptr if it's optional.
        param_copy = param.Copy()
        param_copy.optional = False
        declaration_list.append("const %s" % cpp_util.GetParameterDeclaration(
            param_copy, self._cpp_type_generator.GetCompiledType(param_copy)))
        param_name = param_copy.unix_name
        if param_copy.type_ != param_copy.compiled_type:
          param_name = 'temp_' + param_name
          (c.Append('%s %s;' % (self._cpp_type_generator.GetType(param_copy),
                                param_name))
            .Append(cpp_util.GenerateCompiledTypeToTypeConversion(
                 param_copy,
                 param_copy.unix_name,
                 param_name) + ';')
          )
        c.Append('create_results->Append(%s);' %
            self._CreateValueFromProperty(param_copy, param_name))

      c.Append('return create_results.Pass();')
      c.Eblock('}')
      if generate_to_json:
        c.Append()
        (c.Sblock('std::string %(function_scope)s::'
                  'ToJson(%(declaration_list)s) {')
          .Append('scoped_ptr<base::ListValue> create_results = '
                  '%(function_scope)s::Create(%(param_list)s);')
          .Append('std::string json;')
          .Append('base::JSONWriter::Write(create_results.get(), &json);')
          .Append('return json;')
        )
        c.Eblock('}')

      c.Substitute({
          'function_scope': function_scope,
          'declaration_list': ', '.join(declaration_list),
          'param_list': ', '.join(param.unix_name for param in param_list)
      })

    return c

  def _InitializePropertyToDefault(self, prop, dst):
    """Initialize a model.Property to its default value inside an object.

    E.g for optional enum "state", generate dst->state = STATE_NONE;

    dst: Type*
    """
    c = Code()
    if (self._cpp_type_generator.IsEnumOrEnumRef(prop) or
        prop.type_ == PropertyType.CHOICES):
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
