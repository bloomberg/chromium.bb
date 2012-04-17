# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import os.path
import re

class Model(object):
  """Model of all namespaces that comprise an API.

  Properties:
  - |namespaces| a map of a namespace name to its model.Namespace
  """
  def __init__(self):
    self.namespaces = {}

  def AddNamespace(self, json, source_file):
    """Add a namespace's json to the model and returns the namespace.
    """
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
    self.unix_name = UnixName(self.name)
    self.source_file = source_file
    self.source_file_dir, self.source_file_filename = os.path.split(source_file)
    self.types = {}
    self.functions = {}
    self.parent = None
    # TODO(calamity): Implement properties on namespaces for shared structures
    # or constants across a namespace (e.g Windows::WINDOW_ID_NONE).
    for property_json in json.get('properties', []):
      pass
    for type_json in json.get('types', []):
      type_ = Type(self, type_json['id'], type_json)
      self.types[type_.name] = type_
    for function_json in json.get('functions', []):
      self.functions[function_json['name']] = Function(self, function_json)

class Type(object):
  """A Type defined in the json.

  Properties:
  - |name| the type name
  - |description| the description of the type (if provided)
  - |properties| a map of property unix_names to their model.Property
  - |functions| a map of function names to their model.Function
  - |from_client| indicates that instances of the Type can originate from the
    users of generated code, such as top-level types and function results
  - |from_json| indicates that instances of the Type can originate from the
    JSON (as described by the schema), such as top-level types and function
    parameters
  - |type_| the PropertyType of this Type
  - |item_type| if this is an array, the type of items in the array
  """
  def __init__(self, parent, name, json):
    if json.get('type') == 'array':
      self.type_ = PropertyType.ARRAY
      self.item_type = Property(self, name + "Element", json['items'],
                                from_json=True,
                                from_client=True)
    else:
      if not (
          'properties' in json or
          'additionalProperties' in json or
          'functions' in json):
        raise ParseException(name + " has no properties or functions")
      self.type_ = PropertyType.OBJECT
    self.name = name
    self.description = json.get('description')
    self.from_json = True
    self.from_client = True
    self.properties = {}
    self.functions = {}
    self.parent = parent
    for function_json in json.get('functions', []):
      self.functions[function_json['name']] = Function(self, function_json)
    props = []
    for prop_name, prop_json in json.get('properties', {}).items():
      # TODO(calamity): support functions (callbacks) as properties.  The model
      # doesn't support it yet because the h/cc generators don't -- this is
      # because we'd need to hook it into a base::Callback or something.
      #
      # However, pragmatically it's not necessary to support them anyway, since
      # the instances of functions-on-properties in the extension APIs are all
      # handled in pure Javascript on the render process (and .: never reach
      # C++ let alone the browser).
      if prop_json.get('type') == 'function':
        continue
      props.append(Property(self, prop_name, prop_json,
          from_json=True,
          from_client=True))

    additional_properties = json.get('additionalProperties')
    if additional_properties:
      props.append(Property(self, 'additionalProperties', additional_properties,
          is_additional_properties=True))

    for prop in props:
      if prop.unix_name in self.properties:
        raise ParseException(
            self.properties[prop.unix_name].name + ' and ' + prop.name +
            ' are both named ' + prop.unix_name)
      self.properties[prop.unix_name] = prop

class Callback(object):
  """A callback parameter to a Function.

  Properties:
  - |params| the parameters to this callback.
  """
  def __init__(self, parent, json):
    params = json['parameters']
    self.parent = parent
    self.params = []
    if len(params) == 0:
      return
    elif len(params) == 1:
      param = params[0]
      self.params.append(Property(self, param['name'], param,
          from_client=True))
    else:
      raise ParseException("Callbacks can have at most a single parameter")

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
  def __init__(self, parent, json):
    self.name = json['name']
    self.params = []
    self.description = json.get('description')
    self.callback = None
    self.parent = parent
    for param in json['parameters']:
      if param.get('type') == 'function':
        if self.callback:
          raise ParseException(self.name + " has more than one callback")
        self.callback = Callback(self, param)
      else:
        self.params.append(Property(self, param['name'], param,
            from_json=True))

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

  def __init__(self, parent, name, json, is_additional_properties=False,
      from_json=False, from_client=False):
    """
    Parameters:
    - |from_json| indicates that instances of the Type can originate from the
      JSON (as described by the schema), such as top-level types and function
      parameters
    - |from_client| indicates that instances of the Type can originate from the
      users of generated code, such as top-level types and function results
    """
    self.name = name
    self._unix_name = UnixName(self.name)
    self._unix_name_used = False
    self.optional = json.get('optional', False)
    self.description = json.get('description')
    self.parent = parent
    if is_additional_properties:
      self.type_ = PropertyType.ADDITIONAL_PROPERTIES
    elif '$ref' in json:
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
        self.item_type = Property(self, name + "Element", json['items'],
            from_json=from_json,
            from_client=from_client)
        self.type_ = PropertyType.ARRAY
      elif json_type == 'object':
        self.type_ = PropertyType.OBJECT
        # These members are read when this OBJECT Property is used as a Type
        self.properties = {}
        self.from_json = from_json
        self.from_client = from_client
        type_ = Type(self, self.name, json)
        self.properties = type_.properties
        self.functions = type_.functions
      else:
        raise ParseException(self, 'type ' + json_type + ' not recognized')
    elif 'choices' in json:
      if not json['choices']:
        raise ParseException('Choices has no choices')
      self.choices = {}
      self.type_ = PropertyType.CHOICES
      for choice_json in json['choices']:
        choice = Property(self, self.name, choice_json,
            from_json=from_json,
            from_client=from_client)
        # A choice gets its unix_name set in
        # cpp_type_generator.GetExpandedChoicesInParams
        choice._unix_name = None
        # The existence of any single choice is optional
        choice.optional = True
        self.choices[choice.type_] = choice
    else:
      raise ParseException('Property has no type, $ref or choices')

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
  ADDITIONAL_PROPERTIES = _Info(False, "ADDITIONAL_PROPERTIES")

def UnixName(name):
  """Returns the unix_style name for a given lowerCamelCase string.
  """
  # First replace any lowerUpper patterns with lower_Upper.
  s1 = re.sub('([a-z])([A-Z])', r'\1_\2', name)
  # Now replace any ACMEWidgets patterns with ACME_Widgets
  s2 = re.sub('([A-Z]+)([A-Z][a-z])', r'\1_\2', s1)
  # Finally, replace any remaining periods, and make lowercase.
  return s2.replace('.', '_').lower()

class ParseException(Exception):
  """Thrown when data in the model is invalid."""
  def __init__(self, parent, message):
    hierarchy = GetModelHierarchy(parent)
    hierarchy.append(message)
    Exception.__init__(
        self, 'Model parse exception at:\n' + '\n'.join(hierarchy))

def GetModelHierarchy(entity):
  """Returns the hierarchy of the given model entity."""
  hierarchy = []
  while entity:
    try:
      hierarchy.append(entity.name)
    except AttributeError:
      hierarchy.append(repr(entity))
    entity = entity.parent
  hierarchy.reverse()
  return hierarchy
