# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

class Model(object):
  """Model of all namespaces that comprise an API.
  """
  def __init__(self):
    self.namespaces = {}

  def AddNamespace(self, json, source_file):
    """Add a namespace's json to the model if it has a "compile" property set
    to true. Returns the new namespace or None if a namespace wasn't added.
    """
    if not json.get('compile'):
      return None
    namespace = Namespace(json, source_file)
    self.namespaces[namespace.name] = namespace
    return namespace

class Namespace(object):
  """An API namespace.
  """
  def __init__(self, json, source_file):
    self.name = json['namespace']
    self.source_file = source_file
    self.source_file_dir, self.source_file_filename = os.path.split(source_file)
    self.type_dependencies = {}
    self.types = {}
    self.functions = {}
    for type_json in json['types']:
      type_ = Type(type_json)
      self.types[type_.name] = type_
    for function_json in json['functions']:
      if not function_json.get('nocompile'):
        function = Function(function_json)
        self.functions[function.name] = function

class Type(object):
  """A Type defined in the json.
  """
  def __init__(self, json):
    self.name = json['id']
    self.description = json.get('description')
    self.properties = {}
    for prop_name, prop_json in json['properties'].items():
      self.properties[prop_name] = Property(prop_name, prop_json)

class Callback(object):
  """A callback parameter to a Function.
  """
  def __init__(self, json):
    params = json['parameters']
    if len(params) == 0:
      self.param = None
    elif len(params) == 1:
      param = params[0]
      self.param = Property(param['name'], param)
    else:
      raise AssertionError("Callbacks can have at most a single parameter")

class Function(object):
  """A Function defined in the API.
  """
  def __init__(self, json):
    self.name = json['name']
    self.params = []
    self.description = json['description']
    self.callback = None
    self.type_dependencies = {}
    for param in json['parameters']:
      if param.get('type') == 'function':
        assert (not self.callback), "Function has more than one callback"
        self.callback = Callback(param)
      else:
        self.params.append(Property(param['name'], param))
    assert (self.callback), "Function does not support callback"

# TODO(calamity): handle Enum/choices
class Property(object):
  """A property of a type OR a parameter to a function.

  Members will change based on PropertyType. Check self.type_ to determine which
  members actually exist.
  """
  def __init__(self, name, json):
    self.name = name
    self.optional = json.get('optional', False)
    self.description = json.get('description')
    # TODO(calamity) maybe check for circular refs? could that be a problem?
    if '$ref' in json:
      self.ref_type = json['$ref']
      self.type_ = PropertyType.REF
    elif 'type' in json:
      json_type = json['type']
      if json_type == 'string':
        self.type_ = PropertyType.STRING
      elif json_type ==  'boolean':
        self.type_ = PropertyType.BOOLEAN
      elif json_type == 'integer':
        self.type_ = PropertyType.INTEGER
      elif json_type == 'double':
        self.type_ = PropertyType.DOUBLE
      elif json_type == 'array':
        self.item_type = Property(name + "_inner", json['items'])
        self.type_ = PropertyType.ARRAY
      elif json_type == 'object':
        self.properties = {}
        self.type_ = PropertyType.OBJECT
        for key, val in json['properties'].items():
          self.properties[key] = Property(key, val)
      else:
        raise NotImplementedError(json_type)
    elif 'choices' in json:
      self.type_ = PropertyType.CHOICES
      self.choices = {}

class PropertyType(object):
  """Enum of different types of properties/parameters.
  """
  class _Info(object):
    def __init__(self, is_fundamental):
      self.is_fundamental = is_fundamental

  INTEGER = _Info(True)
  DOUBLE = _Info(True)
  BOOLEAN = _Info(True)
  STRING = _Info(True)
  ARRAY = _Info(False)
  REF = _Info(False)
  CHOICES = _Info(False)
  OBJECT = _Info(False)
