# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilies for the processing of schema python structures.
"""

def StripSchemaNamespace(s):
  last_dot = s.rfind('.')
  if not last_dot == -1:
    return s[last_dot + 1:]
  return s

def PrefixSchemasWithNamespace(schemas):
  for s in schemas:
    _PrefixWithNamespace(s.get("namespace"), s)

def _MaybePrefixFieldWithNamespace(namespace, schema, key):
  if type(schema) == dict and key in schema:
    old_value = schema[key]
    if not "." in old_value:
      schema[key] = namespace + "." + old_value

def _PrefixTypesWithNamespace(namespace, types):
  for t in types:
    _MaybePrefixFieldWithNamespace(namespace, t, "id")
    _MaybePrefixFieldWithNamespace(namespace, t, "customBindings")

def _PrefixWithNamespace(namespace, schema):
  if type(schema) == dict:
    if "types" in schema:
      _PrefixTypesWithNamespace(namespace, schema.get("types"))
    _MaybePrefixFieldWithNamespace(namespace, schema, "$ref")
    for s in schema:
      _PrefixWithNamespace(namespace, schema[s])
  elif type(schema) == list:
    for s in schema:
      _PrefixWithNamespace(namespace, s)
