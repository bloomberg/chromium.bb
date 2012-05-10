#! /usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os.path
import re
import sys

import schema_util

# This file is a peer to json_schema.py. Each of these files understands a
# certain format describing APIs (either JSON or IDL), reads files written
# in that format into memory, and emits them as a Python array of objects
# corresponding to those APIs, where the objects are formatted in a way that
# the JSON schema compiler understands. compiler.py drives both idl_schema.py
# and json_schema.py.

# idl_parser expects to be able to import certain files in its directory,
# so let's set things up the way it wants.
idl_generators_path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                  os.pardir, os.pardir, 'ppapi', 'generators')
if idl_generators_path not in sys.path:
  sys.path.insert(0, idl_generators_path)
import idl_parser

def ProcessComment(comment):
  '''
  Given the string from a Comment node, parse it into a tuple that looks
  like:
    (
      "The processed comment, minus all |parameter| mentions.",
      {
        'parameter_name_1': "The comment that followed |parameter_name_1|:",
        ...
      }
    )
  '''
  # Find all the parameter comments of the form "|name|: comment".
  parameter_comments = re.findall(r'\n *\|([^|]*)\| *: *(.*)', comment)
  # Get the parent comment (everything before the first parameter comment.
  parent_comment = re.sub(r'\n *\|.*', '', comment)
  parent_comment = parent_comment.replace('\n', '').strip()

  parsed = {}
  for (name, comment) in parameter_comments:
    parsed[name] = comment.replace('\n', '').strip()
  return (parent_comment, parsed)

class Callspec(object):
  '''
  Given a Callspec node representing an IDL function declaration, converts into
  a name/value pair where the value is a list of function parameters.
  '''
  def __init__(self, callspec_node, comment):
    self.node = callspec_node
    self.comment = comment

  def process(self, callbacks):
    parameters = []
    for node in self.node.children:
      parameter = Param(node).process(callbacks)
      if parameter['name'] in self.comment:
        parameter['description'] = self.comment[parameter['name']]
      parameters.append(parameter)
    return self.node.GetName(), parameters

class Param(object):
  '''
  Given a Param node representing a function parameter, converts into a Python
  dictionary that the JSON schema compiler expects to see.
  '''
  def __init__(self, param_node):
    self.node = param_node

  def process(self, callbacks):
    return Typeref(self.node.GetProperty( 'TYPEREF'),
                   self.node,
                   { 'name': self.node.GetName() }).process(callbacks)

class Dictionary(object):
  '''
  Given an IDL Dictionary node, converts into a Python dictionary that the JSON
  schema compiler expects to see.
  '''
  def __init__(self, dictionary_node):
    self.node = dictionary_node

  def process(self, callbacks):
    properties = {}
    for node in self.node.children:
      if node.cls == 'Member':
        k, v = Member(node).process(callbacks)
        properties[k] = v
    return { 'id': self.node.GetName(),
             'properties': properties,
             'type': 'object' }

class Member(object):
  '''
  Given an IDL dictionary or interface member, converts into a name/value pair
  where the value is a Python dictionary that the JSON schema compiler expects
  to see.
  '''
  def __init__(self, member_node):
    self.node = member_node

  def process(self, callbacks):
    properties = {}
    name = self.node.GetName()
    if self.node.GetProperty('OPTIONAL'):
      properties['optional'] = True
    if self.node.GetProperty('nodoc'):
      properties['nodoc'] = True
    is_function = False
    parameter_comments = {}
    for node in self.node.children:
      if node.cls == 'Comment':
        (parent_comment, parameter_comments) = ProcessComment(node.GetName())
        properties['description'] = parent_comment
    for node in self.node.children:
      if node.cls == 'Callspec':
        is_function = True
        name, parameters = Callspec(node, parameter_comments).process(callbacks)
        properties['parameters'] = parameters
    properties['name'] = name
    if is_function:
      properties['type'] = 'function'
    else:
      properties = Typeref(self.node.GetProperty('TYPEREF'),
                           self.node, properties).process(callbacks)
    return name, properties

class Typeref(object):
  '''
  Given a TYPEREF property representing the type of dictionary member or
  function parameter, converts into a Python dictionary that the JSON schema
  compiler expects to see.
  '''
  def __init__(self, typeref, parent, additional_properties={}):
    self.typeref = typeref
    self.parent = parent
    self.additional_properties = additional_properties

  def process(self, callbacks):
    properties = self.additional_properties
    result = properties

    if self.parent.GetProperty('OPTIONAL', False):
      properties['optional'] = True

    # The IDL parser denotes array types by adding a child 'Array' node onto
    # the Param node in the Callspec.
    for sibling in self.parent.GetChildren():
      if sibling.cls == 'Array' and sibling.GetName() == self.parent.GetName():
        properties['type'] = 'array'
        properties['items'] = {}
        properties = properties['items']
        break

    if self.typeref == 'DOMString':
      properties['type'] = 'string'
    elif self.typeref == 'boolean':
      properties['type'] = 'boolean'
    elif self.typeref == 'double':
      properties['type'] = 'number'
    elif self.typeref == 'long':
      properties['type'] = 'integer'
    elif self.typeref == 'any':
      properties['type'] = 'any'
    elif self.typeref == 'object':
      properties['type'] = 'object'
      if 'additionalProperties' not in properties:
        properties['additionalProperties'] = {}
      properties['additionalProperties']['type'] = 'any'
      instance_of = self.parent.GetProperty('instanceOf')
      if instance_of:
        properties['isInstanceOf'] = instance_of
    elif self.typeref is None:
      properties['type'] = 'function'
    else:
      if self.typeref in callbacks:
        properties.update(callbacks[self.typeref])
      else:
        properties['$ref'] = self.typeref

    return result

class Namespace(object):
  '''
  Given an IDLNode representing an IDL namespace, converts into a Python
  dictionary that the JSON schema compiler expects to see.
  '''

  def __init__(self, namespace_node, nodoc=False):
    self.namespace = namespace_node
    self.nodoc = nodoc
    self.events = []
    self.functions = []
    self.types = []
    self.callbacks = {}

  def process(self):
    for node in self.namespace.children:
      cls = node.cls
      if cls == "Dictionary":
        self.types.append(Dictionary(node).process(self.callbacks))
      elif cls == "Callback":
        k, v = Member(node).process(self.callbacks)
        self.callbacks[k] = v
      elif cls == "Interface" and node.GetName() == "Functions":
        self.functions = self.process_interface(node)
      elif cls == "Interface" and node.GetName() == "Events":
        self.events = self.process_interface(node)
      else:
        sys.exit("Did not process %s %s" % (node.cls, node))

    return { 'events': self.events,
             'functions': self.functions,
             'types': self.types,
             'namespace': self.namespace.GetName(),
             'nodoc': self.nodoc }

  def process_interface(self, node):
    members = []
    for member in node.children:
      if member.cls == 'Member':
        name, properties = Member(member).process(self.callbacks)
        members.append(properties)
    return members

class IDLSchema(object):
  '''
  Given a list of IDLNodes and IDLAttributes, converts into a Python list
  of api_defs that the JSON schema compiler expects to see.
  '''

  def __init__(self, idl):
    self.idl = idl

  def process(self):
    namespaces = []
    for node in self.idl:
      nodoc = False
      cls = node.cls
      if cls == 'Namespace':
        namespace = Namespace(node, nodoc)
        namespaces.append(namespace.process())
      elif cls == 'Copyright':
        continue
      elif cls == 'Comment':
        continue
      elif cls == 'ExtAttribute':
        if node.name == 'nodoc':
          nodoc = bool(node.value)
        else:
          continue
      else:
        sys.exit("Did not process %s %s" % (node.cls, node))
    schema_util.PrefixSchemasWithNamespace(namespaces)
    return namespaces

def Load(filename):
  '''
  Given the filename of an IDL file, parses it and returns an equivalent
  Python dictionary in a format that the JSON schema compiler expects to see.
  '''

  f = open(filename, 'r')
  contents = f.read()
  f.close()

  idl = idl_parser.IDLParser().ParseData(contents, filename)
  idl_schema = IDLSchema(idl)
  return idl_schema.process()

def Main():
  '''
  Dump a json serialization of parse result for the IDL files whose names
  were passed in on the command line.
  '''
  for filename in sys.argv[1:]:
    schema = Load(filename)
    print json.dumps(schema, indent=2)

if __name__ == '__main__':
  Main()
