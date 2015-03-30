# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Generator that produces an externs file for the Closure Compiler.
Note: This is a work in progress, and generated externs may require tweaking.

See https://developers.google.com/closure/compiler/docs/api-tutorial3#externs
"""

from code import Code
from model import *
from schema_util import *

import os
from datetime import datetime

LICENSE = ("""// Copyright %s The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
""" % datetime.now().year)

class JsExternsGenerator(object):
  def Generate(self, namespace):
    return _Generator(namespace).Generate()

class _Generator(object):
  def __init__(self, namespace):
    self._namespace = namespace

  def Generate(self):
    """Generates a Code object with the schema for the entire namespace.
    """
    c = Code()
    (c.Append(LICENSE)
        .Append()
        .Append('/** @fileoverview Externs generated from namespace: %s */' %
                self._namespace.name)
        .Append())

    c.Cblock(self._GenerateNamespaceObject())

    for js_type in self._namespace.types.values():
      c.Cblock(self._GenerateType(js_type))

    for function in self._namespace.functions.values():
      c.Cblock(self._GenerateFunction(function))

    for event in self._namespace.events.values():
      c.Cblock(self._GenerateEvent(event))

    return c

  def _GenerateType(self, js_type):
    """Given a Type object, returns the Code for this type's definition.
    """
    c = Code()
    if js_type.property_type is PropertyType.ENUM:
      c.Concat(self._GenerateEnumJsDoc(js_type))
    else:
      c.Concat(self._GenerateTypeJsDoc(js_type))

    return c

  def _GenerateEnumJsDoc(self, js_type):
    """ Given an Enum Type object, returns the Code for the enum's definition.
    """
    c = Code()
    (c.Sblock(line='/**', line_prefix=' * ')
      .Append('@enum {string}')
      .Append(self._GenerateSeeLink('type', js_type.simple_name))
      .Eblock(' */'))
    c.Append('chrome.%s.%s = {' % (self._namespace.name, js_type.name))
    c.Append('\n'.join(
        ["  %s: '%s'," % (v.name, v.name) for v in js_type.enum_values]))
    c.Append('};')
    return c

  def _IsTypeConstructor(self, js_type):
    """Returns true if the given type should be a @constructor. If this returns
       false, the type is a typedef.
    """
    return any(prop.type_.property_type is PropertyType.FUNCTION
               for prop in js_type.properties.values())

  def _GenerateTypeJsDoc(self, js_type):
    """Generates the documentation for a type as a Code.

    Returns an empty code object if the object has no documentation.
    """
    c = Code()
    c.Sblock(line='/**', line_prefix=' * ')

    if js_type.description:
      for line in js_type.description.splitlines():
        c.Append(line)

    is_constructor = self._IsTypeConstructor(js_type)
    if is_constructor:
      c.Comment('@constructor', comment_prefix = ' * ', wrap_indent=4)
    else:
      c.Concat(self._GenerateTypedef(js_type.properties))

    c.Append(self._GenerateSeeLink('type', js_type.simple_name))
    c.Eblock(' */')

    var = 'var ' + js_type.simple_name
    if is_constructor: var += ' = function() {}'
    var += ';'
    c.Append(var)

    return c

  def _GenerateTypedef(self, properties):
    """Given an OrderedDict of properties, returns a Code containing a @typedef.
    """
    if not properties: return Code()

    c = Code()
    c.Append('@typedef {')
    c.Concat(self._GenerateObjectDefinition(properties), new_line=False)
    c.Append('}', new_line=False)
    return c

  def _GenerateObjectDefinition(self, properties):
    """Given an OrderedDict of properties, returns a Code containing the
       description of an object.
    """
    if not properties: return ''

    c = Code()
    c.Sblock('{')
    first = True
    for field, prop in properties.items():
      # Avoid trailing comma.
      # TODO(devlin): This will be unneeded, if/when
      # https://github.com/google/closure-compiler/issues/796 is fixed.
      if not first:
        c.Append(',', new_line=False)
      first = False
      js_type = self._TypeToJsType(prop.type_)
      if prop.optional:
        js_type = (Code().
                       Append('(').
                       Concat(js_type, new_line=False).
                       Append('|undefined)', new_line=False))
      c.Append('%s: ' % field, strip_right=False)
      c.Concat(js_type, new_line=False)

    c.Eblock('}')

    return c

  def _GenerateFunctionJsDoc(self, function):
    """Generates the documentation for a function as a Code.

    Returns an empty code object if the object has no documentation.
    """
    c = Code()
    c.Sblock(line='/**', line_prefix=' * ')

    if function.description:
      c.Comment(function.description, comment_prefix='')

    def append_field(c, tag, js_type, name, optional, description):
      c.Append('@%s {' % tag)
      c.Concat(js_type, new_line=False)
      if optional:
        c.Append('=', new_line=False)
      c.Append('} %s' % name, new_line=False)
      if description:
        c.Comment(' %s' % description, comment_prefix='',
                  wrap_indent=4, new_line=False)

    for param in function.params:
      append_field(c, 'param', self._TypeToJsType(param.type_), param.name,
                   param.optional, param.description)

    if function.callback:
      append_field(c, 'param', self._FunctionToJsFunction(function.callback),
                   function.callback.name, function.callback.optional,
                   function.callback.description)

    if function.returns:
      append_field(c, 'return', self._TypeToJsType(function.returns),
                   '', False, function.returns.description)

    if function.deprecated:
      c.Append('@deprecated %s' % function.deprecated)

    c.Append(self._GenerateSeeLink('method', function.name))

    c.Eblock(' */')
    return c

  def _FunctionToJsFunction(self, function):
    """Converts a model.Function to a JS type (i.e., function([params])...)"""
    c = Code()
    c.Append('function(')
    for i, param in enumerate(function.params):
      c.Concat(self._TypeToJsType(param.type_), new_line=False)
      if i is not len(function.params) - 1:
        c.Append(', ', new_line=False, strip_right=False)
    c.Append('):', new_line=False)

    if function.returns:
      c.Concat(self._TypeToJsType(function.returns), new_line=False)
    else:
      c.Append('void', new_line=False)

    return c

  def _TypeToJsType(self, js_type):
    """Converts a model.Type to a JS type (number, Array, etc.)"""
    c = Code()
    if js_type.property_type in (PropertyType.INTEGER, PropertyType.DOUBLE):
      c.Append('number')
    elif js_type.property_type is PropertyType.OBJECT:
      c = self._GenerateObjectDefinition(js_type.properties)
    elif js_type.property_type is PropertyType.ARRAY:
      (c.Append('!Array<').
           Concat(self._TypeToJsType(js_type.item_type), new_line=False).
           Append('>', new_line=False))
    elif js_type.property_type is PropertyType.REF:
      ref_type = js_type.ref_type
      # Enums are defined as chrome.fooAPI.MyEnum, but types are defined simply
      # as MyType.
      if self._namespace.types[ref_type].property_type is PropertyType.ENUM:
        ref_type = '!chrome.%s.%s' % (self._namespace.name, ref_type)
      c.Append(ref_type)
    elif js_type.property_type is PropertyType.CHOICES:
      c.Append('(')
      for i, choice in enumerate(js_type.choices):
        c.Concat(self._TypeToJsType(choice), new_line=False)
        if i is not len(js_type.choices) - 1:
          c.Append('|', new_line=False)
      c.Append(')', new_line=False)
    elif js_type.property_type is PropertyType.FUNCTION:
      c = self._FunctionToJsFunction(js_type.function)
    elif js_type.property_type is PropertyType.ANY:
      c.Append('*')
    elif js_type.property_type.is_fundamental:
      c.Append(js_type.property_type.name)
    else:
      c.Append('?') # TODO(tbreisacher): Make this more specific.
    return c

  def _GenerateFunction(self, function):
    """Generates the code representing a function, including its documentation.
       For example:

       /**
        * @param {string} title The new title.
        */
       chrome.window.setTitle = function(title) {};
    """
    c = Code()
    params = self._GenerateFunctionParams(function)
    (c.Concat(self._GenerateFunctionJsDoc(function))
      .Append('chrome.%s.%s = function(%s) {};' % (self._namespace.name,
                                                   function.name,
                                                   params))
    )
    return c

  def _GenerateEvent(self, event):
    """Generates the code representing an event.
       For example:

       /** @type {!ChromeEvent} */
       chrome.bookmarks.onChildrenReordered;
    """
    c = Code()
    c.Sblock(line='/**', line_prefix=' * ')
    if (event.description):
      c.Comment(event.description, comment_prefix='')
    c.Append('@type {!ChromeEvent}')
    c.Append(self._GenerateSeeLink('event', event.name))
    c.Eblock(' */')
    c.Append('chrome.%s.%s;' % (self._namespace.name, event.name))
    return c

  def _GenerateNamespaceObject(self):
    """Generates the code creating namespace object.
       For example:

       /**
        * @const
        */
       chrome.bookmarks = {};
    """
    c = Code()
    (c.Append("""/**
 * @const
 */""")
      .Append('chrome.%s = {};' % self._namespace.name))
    return c

  def _GenerateFunctionParams(self, function):
    params = function.params[:]
    if function.callback:
      params.append(function.callback)
    return ', '.join(param.name for param in params)

  def _GenerateSeeLink(self, object_type, object_name):
    """Generates a @see link for a given API 'object' (type, method, or event).
    """

    # NOTE(devlin): This is kind of a hack. Some APIs will be hosted on
    # developer.chrome.com/apps/ instead of /extensions/, and some APIs have
    # '.'s in them (like app.window), which should resolve to 'app_window'.
    # Luckily, the doc server has excellent url resolution, and knows exactly
    # what we mean. This saves us from needing any complicated logic here.
    return ('@see https://developer.chrome.com/extensions/%s#%s-%s' %
                (self._namespace.name, object_type, object_name))
