# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json

def StripJSONComments(stream):
  """Strips //-style comments from a stream of JSON. Allows un-escaped //
  inside string values.
  """
  # Previously we used json_minify to strip comments, but it seems to be pretty
  # slow and does more than we need. This implementation does a lot less work -
  # it just strips comments from the beginning of the '//' delimiter until end
  # of line, but only if we're not inside a string. For example:
  #
  #  {"url": "http://www.example.com"}
  #
  # will work properly, as will:
  #
  #  {
  #    "url": "http://www.example.com" // some comment
  #  }
  result = ""
  last_char = None
  inside_string = False
  inside_comment = False
  buf = ""
  for char in stream:
    if inside_comment:
      if char == '\n':
        inside_comment = False
      else:
        continue
    else:
      if char == '/' and not inside_string:
        if last_char == '/':
          inside_comment = True
        last_char = char
        continue
      else:
        if last_char == '/' and not inside_string:
          result += '/'
        if char == '"':
          inside_string = not inside_string
    last_char = char
    result += char

  return result

def Load(filename):
  with open(filename, 'r') as handle:
    return json.loads(StripJSONComments(handle.read()))


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
