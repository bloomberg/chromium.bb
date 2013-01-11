# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import os.path
import re

from json_parse import OrderedDict

class ParseException(Exception):
  """Thrown when data in the model is invalid.
  """
  def __init__(self, parent, message):
    hierarchy = _GetModelHierarchy(parent)
    hierarchy.append(message)
    Exception.__init__(
        self, 'Model parse exception at:\n' + '\n'.join(hierarchy))

class Model(object):
  """Model of all namespaces that comprise an API.

  Properties:
  - |namespaces| a map of a namespace name to its model.Namespace
  """
  def __init__(self):
    self.namespaces = {}

  def AddNamespace(self, json, source_file, include_compiler_options=False):
    """Add a namespace's json to the model and returns the namespace.
    """
    namespace = Namespace(json,
                          source_file,
                          include_compiler_options=include_compiler_options)
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
  - |platforms| if not None, the list of platforms that the namespace is
                available to
  - |types| a map of type names to their model.Type
  - |functions| a map of function names to their model.Function
  - |events| a map of event names to their model.Function
  - |properties| a map of property names to their model.Property
  - |compiler_options| the compiler_options dict, only present if
                       |include_compiler_options| is True
  """
  def __init__(self, json, source_file, include_compiler_options=False):
    self.name = json['namespace']
    self.unix_name = UnixName(self.name)
    self.source_file = source_file
    self.source_file_dir, self.source_file_filename = os.path.split(source_file)
    self.parent = None
    self.platforms = _GetPlatforms(json)
    _AddTypes(self, json, self)
    _AddFunctions(self, json, self)
    _AddEvents(self, json, self)
    _AddProperties(self, json, self)
    if include_compiler_options:
      self.compiler_options = json.get('compiler_options', {})

class Type(object):
  """A Type defined in the json.

  Properties:
  - |name| the type name
  - |description| the description of the type (if provided)
  - |properties| a map of property unix_names to their model.Property
  - |functions| a map of function names to their model.Function
  - |events| a map of event names to their model.Event
  - |from_client| indicates that instances of the Type can originate from the
    users of generated code, such as top-level types and function results
  - |from_json| indicates that instances of the Type can originate from the
    JSON (as described by the schema), such as top-level types and function
    parameters
  - |type_| the PropertyType of this Type
  - |item_type| if this is an array, the type of items in the array
  - |simple_name| the name of this Type without a namespace
  """
  def __init__(self, parent, name, json, namespace):
    if json.get('type') == 'array':
      self.type_ = PropertyType.ARRAY
      self.item_type = Property(self,
                                name + "Element",
                                json['items'],
                                namespace,
                                from_json=True,
                                from_client=True)
    elif 'enum' in json:
      self.enum_values = []
      for value in json['enum']:
        self.enum_values.append(value)
      self.type_ = PropertyType.ENUM
    elif json.get('type') == 'string':
      self.type_ = PropertyType.STRING
    else:
      if not (
          'properties' in json or
          'additionalProperties' in json or
          'functions' in json or
          'events' in json):
        raise ParseException(self, name + " has no properties or functions")
      self.type_ = PropertyType.OBJECT
    self.name = name
    self.simple_name = _StripNamespace(self.name, namespace)
    self.unix_name = UnixName(self.name)
    self.description = json.get('description')
    self.from_json = True
    self.from_client = True
    self.parent = parent
    self.instance_of = json.get('isInstanceOf', None)
    _AddFunctions(self, json, namespace)
    _AddEvents(self, json, namespace)
    _AddProperties(self, json, namespace, from_json=True, from_client=True)

    additional_properties_key = 'additionalProperties'
    additional_properties = json.get(additional_properties_key)
    if additional_properties:
      self.properties[additional_properties_key] = Property(
          self,
          additional_properties_key,
          additional_properties,
          namespace,
          is_additional_properties=True)

class Function(object):
  """A Function defined in the API.

  Properties:
  - |name| the function name
  - |platforms| if not None, the list of platforms that the function is
                available to
  - |params| a list of parameters to the function (order matters). A separate
             parameter is used for each choice of a 'choices' parameter
  - |description| a description of the function (if provided)
  - |callback| the callback parameter to the function. There should be exactly
               one
  - |optional| whether the Function is "optional"; this only makes sense to be
               present when the Function is representing a callback property
  - |simple_name| the name of this Function without a namespace
  """
  def __init__(self,
               parent,
               json,
               namespace,
               from_json=False,
               from_client=False):
    self.name = json['name']
    self.simple_name = _StripNamespace(self.name, namespace)
    self.platforms = _GetPlatforms(json)
    self.params = []
    self.description = json.get('description')
    self.callback = None
    self.optional = json.get('optional', False)
    self.parent = parent
    self.nocompile = json.get('nocompile')
    options = json.get('options', {})
    self.conditions = options.get('conditions', [])
    self.actions = options.get('actions', [])
    self.supports_listeners = options.get('supportsListeners', True)
    self.supports_rules = options.get('supportsRules', False)
    def GeneratePropertyFromParam(p):
      return Property(self,
                      p['name'], p,
                      namespace,
                      from_json=from_json,
                      from_client=from_client)

    self.filters = [GeneratePropertyFromParam(filter)
                    for filter in json.get('filters', [])]
    callback_param = None
    for param in json.get('parameters', []):

      if param.get('type') == 'function':
        if callback_param:
          # No ParseException because the webstore has this.
          # Instead, pretend all intermediate callbacks are properties.
          self.params.append(GeneratePropertyFromParam(callback_param))
        callback_param = param
      else:
        self.params.append(GeneratePropertyFromParam(param))

    if callback_param:
      self.callback = Function(self,
                               callback_param,
                               namespace,
                               from_client=True)

    self.returns = None
    if 'returns' in json:
      self.returns = Property(self, 'return', json['returns'], namespace)

class Property(object):
  """A property of a type OR a parameter to a function.

  Properties:
  - |name| name of the property as in the json. This shouldn't change since
    it is the key used to access DictionaryValues
  - |unix_name| the unix_style_name of the property. Used as variable name
  - |optional| a boolean representing whether the property is optional
  - |description| a description of the property (if provided)
  - |type_| the model.PropertyType of this property
  - |compiled_type| the model.PropertyType that this property should be
    compiled to from the JSON. Defaults to |type_|.
  - |ref_type| the type that the REF property is referencing. Can be used to
    map to its model.Type
  - |item_type| a model.Property representing the type of each element in an
    ARRAY
  - |properties| the properties of an OBJECT parameter
  - |from_client| indicates that instances of the Type can originate from the
    users of generated code, such as top-level types and function results
  - |from_json| indicates that instances of the Type can originate from the
    JSON (as described by the schema), such as top-level types and function
    parameters
  - |simple_name| the name of this Property without a namespace
  """

  def __init__(self,
               parent,
               name,
               json,
               namespace,
               is_additional_properties=False,
               from_json=False,
               from_client=False):
    self.name = name
    self.simple_name = _StripNamespace(self.name, namespace)
    self._unix_name = UnixName(self.name)
    self._unix_name_used = False
    self.optional = json.get('optional', False)
    self.functions = OrderedDict()
    self.has_value = False
    self.description = json.get('description')
    self.parent = parent
    self.from_json = from_json
    self.from_client = from_client
    self.instance_of = json.get('isInstanceOf', None)
    self.params = []
    self.returns = None
    _AddProperties(self, json, namespace)
    if is_additional_properties:
      self.type_ = PropertyType.ADDITIONAL_PROPERTIES
    elif '$ref' in json:
      self.ref_type = json['$ref']
      self.type_ = PropertyType.REF
    elif 'enum' in json and json.get('type') == 'string':
      # Non-string enums (as in the case of [legalValues=(1,2)]) should fall
      # through to the next elif.
      self.enum_values = []
      for value in json['enum']:
        self.enum_values.append(value)
      self.type_ = PropertyType.ENUM
    elif 'type' in json:
      self.type_ = self._JsonTypeToPropertyType(json['type'])
      if self.type_ == PropertyType.ARRAY:
        self.item_type = Property(self,
                                  name + "Element",
                                  json['items'],
                                  namespace,
                                  from_json=from_json,
                                  from_client=from_client)
      elif self.type_ == PropertyType.OBJECT:
        # These members are read when this OBJECT Property is used as a Type
        type_ = Type(self, self.name, json, namespace)
        # self.properties will already have some value from |_AddProperties|.
        self.properties.update(type_.properties)
        self.functions = type_.functions
      elif self.type_ == PropertyType.FUNCTION:
        for p in json.get('parameters', []):
          self.params.append(Property(self,
                                      p['name'],
                                      p,
                                      namespace,
                                      from_json=from_json,
                                      from_client=from_client))
        if 'returns' in json:
          self.returns = Property(self, 'return', json['returns'], namespace)
    elif 'choices' in json:
      if not json['choices'] or len(json['choices']) == 0:
        raise ParseException(self, 'Choices has no choices')
      self.choices = {}
      self.type_ = PropertyType.CHOICES
      self.compiled_type = self.type_
      for choice_json in json['choices']:
        choice = Property(self,
                          self.name,
                          choice_json,
                          namespace,
                          from_json=from_json,
                          from_client=from_client)
        choice.unix_name = UnixName(self.name + choice.type_.name)
        # The existence of any single choice is optional
        choice.optional = True
        self.choices[choice.type_] = choice
    elif 'value' in json:
      self.has_value = True
      self.value = json['value']
      if type(self.value) == int:
        self.type_ = PropertyType.INTEGER
        self.compiled_type = self.type_
      else:
        # TODO(kalman): support more types as necessary.
        raise ParseException(
            self, '"%s" is not a supported type' % type(self.value))
    else:
      raise ParseException(
          self, 'Property has no type, $ref, choices, or value')
    if 'compiled_type' in json:
      if 'type' in json:
        self.compiled_type = self._JsonTypeToPropertyType(json['compiled_type'])
      else:
        raise ParseException(self, 'Property has compiled_type but no type')
    else:
      self.compiled_type = self.type_

  def _JsonTypeToPropertyType(self, json_type):
    try:
      return {
        'any': PropertyType.ANY,
        'array': PropertyType.ARRAY,
        'binary': PropertyType.BINARY,
        'boolean': PropertyType.BOOLEAN,
        'integer': PropertyType.INTEGER,
        'int64': PropertyType.INT64,
        'function': PropertyType.FUNCTION,
        'number': PropertyType.DOUBLE,
        'object': PropertyType.OBJECT,
        'string': PropertyType.STRING,
      }[json_type]
    except KeyError:
      raise NotImplementedError('Type %s not recognized' % json_type)

  def GetUnixName(self):
    """Gets the property's unix_name. Raises AttributeError if not set.
    """
    if not self._unix_name:
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

class _Enum(object):
  """Superclass for enum types with a "name" field, setting up repr/eq/ne.
  Enums need to do this so that equality/non-equality work over pickling.
  """

  @staticmethod
  def GetAll(cls):
    """Yields all _Enum objects declared in |cls|.
    """
    for prop_key in dir(cls):
      prop_value = getattr(cls, prop_key)
      if isinstance(prop_value, _Enum):
        yield prop_value

  def __init__(self, name):
    self.name = name

  def __repr(self):
    return self.name

  def __eq__(self, other):
    return type(other) == type(self) and other.name == self.name

  def __ne__(self, other):
    return not (self == other)

class _PropertyTypeInfo(_Enum):
  def __init__(self, is_fundamental, name):
    _Enum.__init__(self, name)
    self.is_fundamental = is_fundamental

class PropertyType(object):
  """Enum of different types of properties/parameters.
  """
  INTEGER = _PropertyTypeInfo(True, "INTEGER")
  INT64 = _PropertyTypeInfo(True, "INT64")
  DOUBLE = _PropertyTypeInfo(True, "DOUBLE")
  BOOLEAN = _PropertyTypeInfo(True, "BOOLEAN")
  STRING = _PropertyTypeInfo(True, "STRING")
  ENUM = _PropertyTypeInfo(False, "ENUM")
  ARRAY = _PropertyTypeInfo(False, "ARRAY")
  REF = _PropertyTypeInfo(False, "REF")
  CHOICES = _PropertyTypeInfo(False, "CHOICES")
  OBJECT = _PropertyTypeInfo(False, "OBJECT")
  FUNCTION = _PropertyTypeInfo(False, "FUNCTION")
  BINARY = _PropertyTypeInfo(False, "BINARY")
  ANY = _PropertyTypeInfo(False, "ANY")
  ADDITIONAL_PROPERTIES = _PropertyTypeInfo(False, "ADDITIONAL_PROPERTIES")

def UnixName(name):
  """Returns the unix_style name for a given lowerCamelCase string.
  """
  # First replace any lowerUpper patterns with lower_Upper.
  s1 = re.sub('([a-z])([A-Z])', r'\1_\2', name)
  # Now replace any ACMEWidgets patterns with ACME_Widgets
  s2 = re.sub('([A-Z]+)([A-Z][a-z])', r'\1_\2', s1)
  # Finally, replace any remaining periods, and make lowercase.
  return s2.replace('.', '_').lower()

def _StripNamespace(name, namespace):
  if name.startswith(namespace.name + '.'):
    return name[len(namespace.name + '.'):]
  return name

def _GetModelHierarchy(entity):
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

def _AddTypes(model, json, namespace):
  """Adds Type objects to |model| contained in the 'types' field of |json|.
  """
  model.types = OrderedDict()
  for type_json in json.get('types', []):
    type_ = Type(model, type_json['id'], type_json, namespace)
    model.types[type_.name] = type_

def _AddFunctions(model, json, namespace):
  """Adds Function objects to |model| contained in the 'functions' field of
  |json|.
  """
  model.functions = OrderedDict()
  for function_json in json.get('functions', []):
    function = Function(model, function_json, namespace, from_json=True)
    model.functions[function.name] = function

def _AddEvents(model, json, namespace):
  """Adds Function objects to |model| contained in the 'events' field of |json|.
  """
  model.events = OrderedDict()
  for event_json in json.get('events', []):
    event = Function(model, event_json, namespace, from_client=True)
    model.events[event.name] = event

def _AddProperties(model,
                   json,
                   namespace,
                   from_json=False,
                   from_client=False):
  """Adds model.Property objects to |model| contained in the 'properties' field
  of |json|.
  """
  model.properties = OrderedDict()
  for name, property_json in json.get('properties', {}).items():
    model.properties[name] = Property(
        model,
        name,
        property_json,
        namespace,
        from_json=from_json,
        from_client=from_client)

class _PlatformInfo(_Enum):
  def __init__(self, name):
    _Enum.__init__(self, name)

class Platforms(object):
  """Enum of the possible platforms.
  """
  CHROMEOS = _PlatformInfo("chromeos")
  CHROMEOS_TOUCH = _PlatformInfo("chromeos_touch")
  LINUX = _PlatformInfo("linux")
  MAC = _PlatformInfo("mac")
  WIN = _PlatformInfo("win")

def _GetPlatforms(json):
  if 'platforms' not in json:
    return None
  platforms = []
  for platform_name in json['platforms']:
    for platform_enum in _Enum.GetAll(Platforms):
      if platform_name == platform_enum.name:
        platforms.append(platform_enum)
        break
  return platforms
