# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json
import os.path
import sys

_script_path = os.path.realpath(__file__)
sys.path.insert(0, os.path.normpath(_script_path + "/../../"))
import json_comment_eater
import schema_util

def DeleteNocompileNodes(item):
  def HasNocompile(thing):
    return type(thing) == dict and thing.get('nocompile', False)

  if type(item) == dict:
    toDelete = []
    for key, value in item.items():
      if HasNocompile(value):
        toDelete.append(key)
      else:
        DeleteNocompileNodes(value)
    for key in toDelete:
      del item[key]
  elif type(item) == list:
    item[:] = [DeleteNocompileNodes(thing)
        for thing in item if not HasNocompile(thing)]

  return item

def Load(filename):
  with open(filename, 'r') as handle:
    schemas = DeleteNocompileNodes(
        json.loads(json_comment_eater.Nom(handle.read())))
  schema_util.PrefixSchemasWithNamespace(schemas)
  return schemas

# A dictionary mapping |filename| to the object resulting from loading the JSON
# at |filename|.
_cache = {}

def CachedLoad(filename):
  """Equivalent to Load(filename), but caches results for subsequent calls"""
  if filename not in _cache:
    _cache[filename] = Load(filename)
  # Return a copy of the object so that any changes a caller makes won't affect
  # the next caller.
  return copy.deepcopy(_cache[filename])
