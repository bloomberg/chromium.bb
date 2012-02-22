# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import re
import copy

class Model(object):
  """Model of all namespaces that comprise an API.

  Properties:
  - |namespaces| a map of a namespace name to its model.Namespace
  """
  def __init__(self):
    self.namespaces = {}

  def AddNamespace(self, json, source_file):
    """Add a namespace's json to the model if it doesn't have "nocompile"
    property set to true. Returns the new namespace or None if a namespace
    wasn't added.
    """
    if json.get('nocompile', False):
      return None
    namespace = Namespace(json, source_file)
    self.namespaces[namespace.name] = namespace
    return namespace

class Namespace(object):
  """An API namespace.

  Properties:
  - |name| the name of the namespace
  - |unix_name| the unix_name of the namespace
  - |source_file| the file that contained the namespace definition
  - |source_file_dir| the directory component of |source_file|
  - |source_file_filename| the filename component of |source_file|
  - |types| a map of type names to their model.Type
  - |functions| a map of function names to their model.Function
  """
  def __init__(self, json, source_file):
    self.name = json['namespace']
    self.unix_name = _UnixName(self.name)
    self.source_file = source_file
    self.source_file_dir, self.source_file_filename = os.path.split(source_file)
    self.types = {}
    self.functions = {}
    for type_json in json.get('types', []):
      type_ = Type(type_json)
      self.types[type_.name] = type_
    for function_json in json.get('functions', []):
      if not function_json.get('nocompile', False):
        function = Function(function_json)
        self.functions[function.name] = function

class Type(object):
  """A Type defined in the json.

  Properties:
  - |name| the type name
  - |description| the description of the type (if provided)
  - |properties| a map of property names to their model.Property
  """
  def __init__(self, json):
    self.name = json['id']
    self.description = json.get('description')
    self.properties = {}
    for prop_name, prop_json in json['properties'].items():
      self.properties[prop_name] = Property(prop_name, prop_json)

class Callback(object):
  """A callback parameter to a Function.

  Properties:
  - |params| the parameters to this callback.
  """
  def __init__(self, json):
    params = json['parameters']
    self.params = []
    if len(params) == 0:
      return
    elif len(params) == 1:
      param = params[0]
      self.params.append(Property(param['name'], param))
    else:
      raise AssertionError("Callbacks can have at most a single parameter")

class Function(object):
  """A Function defined in the API.

  Properties:
  - |name| the function name
  - |params| a list of parameters to the function (order matters). A separate
    parameter is used for each choice of a 'choices' parameter.
  - |description| a description of the function (if provided)
  - |callback| the callback parameter to the function. There should be exactly
    one
  """
  def __init__(self, json):
    self.name = json['name']
    self.params = []
    self.description = json['description']
    self.callback = None
    for param in json['parameters']:
      if param.get('type') == 'function':
        assert (not self.callback), self.name + " has more than one callback"
        self.callback = Callback(param)
      else:
        self.params.append(Property(param['name'], param))
    assert (self.callback), self.name + " does not support callback"

class Property(object):
  """A property of a type OR a parameter to a function.

  Properties:
  - |name| name of the property as in the json. This shouldn't change since
    it is the key used to access DictionaryValues
  - |unix_name| the unix_style_name of the property. Used as variable name
  - |optional| a boolean representing whether the property is optional
  - |description| a description of the property (if provided)
  - |type_| the model.PropertyType of this property
  - |ref_type| the type that the REF property is referencing. Can be used to
    map to its model.Type
  - |item_type| a model.Property representing the type of each element in an
    ARRAY
  - |properties| the properties of an OBJECT parameter
  """
  def __init__(self, name, json):
    if not re.match('^[a-z][a-zA-Z0-9]*$', name):
      raise AssertionError('Name %s must be lowerCamelCase' % name)
    self.name = name
    self._unix_name = _UnixName(self.name)
    self._unix_name_used = False
    self.optional = json.get('optional', False)
    self.description = json.get('description')
    if '$ref' in json:
      self.ref_type = json['$ref']
      self.type_ = PropertyType.REF
    elif 'enum' in json:
      self.enum_values = []
      for value in json['enum']:
        self.enum_values.append(value)
      self.type_ = PropertyType.ENUM
    elif 'type' in json:
      json_type = json['type']
      if json_type == 'string':
        self.type_ = PropertyType.STRING
      elif json_type == 'any':
        self.type_ = PropertyType.ANY
      elif json_type == 'boolean':
        self.type_ = PropertyType.BOOLEAN
      elif json_type == 'integer':
        self.type_ = PropertyType.INTEGER
      elif json_type == 'number':
        self.type_ = PropertyType.DOUBLE
      elif json_type == 'array':
        self.item_type = Property(name + "Element", json['items'])
        self.type_ = PropertyType.ARRAY
      elif json_type == 'object':
        self.properties = {}
        self.type_ = PropertyType.OBJECT
        for key, val in json['properties'].items():
          self.properties[key] = Property(key, val)
      else:
        raise NotImplementedError(json_type)
    elif 'choices' in json:
      assert len(json['choices']), 'Choices has no choices\n%s' % json
      self.choices = {}
      self.type_ = PropertyType.CHOICES
      for choice_json in json['choices']:
        choice = Property(self.name, choice_json)
        # A choice gets its unix_name set in
        # cpp_type_generator.GetExpandedChoicesInParams
        choice._unix_name = None
        # The existence of any single choice is optional
        choice.optional = True
        self.choices[choice.type_] = choice
    else:
      raise NotImplementedError(json)

  def GetUnixName(self):
    """Gets the property's unix_name. Raises AttributeError if not set.
    """
    if self._unix_name is None:
      raise AttributeError('No unix_name set on %s' % self.name)
    self._unix_name_used = True
    return self._unix_name

  def SetUnixName(self, unix_name):
    """Set the property's unix_name. Raises AttributeError if the unix_name has
    already been used (GetUnixName has been called).
    """
    if unix_name == self._unix_name:
      return
    if self._unix_name_used:
      raise AttributeError(
          'Cannot set the unix_name on %s; '
          'it is already used elsewhere as %s' %
          (self.name, self._unix_name))
    self._unix_name = unix_name

  def Copy(self):
    """Makes a copy of this model.Property object and allow the unix_name to be
    set again.
    """
    property_copy = copy.copy(self)
    property_copy._unix_name_used = False
    return property_copy

  unix_name = property(GetUnixName, SetUnixName)

class PropertyType(object):
  """Enum of different types of properties/parameters.
  """
  class _Info(object):
    def __init__(self, is_fundamental, name):
      self.is_fundamental = is_fundamental
      self.name = name

    def __repr__(self):
      return self.name

  INTEGER = _Info(True, "INTEGER")
  DOUBLE = _Info(True, "DOUBLE")
  BOOLEAN = _Info(True, "BOOLEAN")
  STRING = _Info(True, "STRING")
  ENUM = _Info(False, "ENUM")
  ARRAY = _Info(False, "ARRAY")
  REF = _Info(False, "REF")
  CHOICES = _Info(False, "CHOICES")
  OBJECT = _Info(False, "OBJECT")
  ANY = _Info(False, "ANY")

def _UnixName(name):
  """Returns the unix_style name for a given lowerCamelCase string.
  """
  return '_'.join([x.lower()
      for x in re.findall('[A-Z][a-z_]*', name[0].upper() + name[1:])])

