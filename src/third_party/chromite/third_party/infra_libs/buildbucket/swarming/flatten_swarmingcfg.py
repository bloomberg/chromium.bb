# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code to flatten a swarming config, specifically the buildbucket builders.

There are several features in the proto that can be used to reduce code
verbosity:
  * builder defaults
  * builder mixins
  * recipe properties (instead of properties_j)

This code exercises those features and produces a flattened config proto.
"""

import copy
import json

## Public API.


def read_properties(recipe):
  """Parses build properties from the recipe message.

  Expects the message to be valid.

  Uses NO_PROPERTY for empty values.
  """
  result = dict(p.split(':', 1) for p in recipe.properties)
  for p in recipe.properties_j:
    k, v = p.split(':', 1)
    parsed = json.loads(v)
    result[k] = parsed
  return result


def parse_dimensions(strings):
  """Parses dimension strings to a dict {key: (value, expiration_sec)}.

  Repeated dimension keys are not supported.
  """
  out = {}
  for s in strings:
    key, value = s.split(':', 1)
    expiration_secs = 0
    try:
      expiration_secs = int(key)
    except ValueError:
      pass
    else:
      key, value = value.split(':', 1)
    out[key] = (value, expiration_secs)
  return out


def format_dimensions(dictionary):
  """Formats a dictionary of dimensions to a list of strings.

  Opposite of parse_dimensions.
  """
  out = []
  for key, (value, expiration_secs) in dictionary.iteritems():
    if expiration_secs:
      out.append('%d:%s:%s' % (expiration_secs, key, value))
    else:
      out.append('%s:%s' % (key, value))
  out.sort()
  return out


def merge_builder(b1, b2):
  """Merges Builder message b2 into b1. Expects messages to be valid.

  Repeated dimension keys are not supported.
  """
  assert not b2.mixins, 'do not merge unflattened builders'

  dims = parse_dimensions(b1.dimensions)
  dims.update(parse_dimensions(b2.dimensions))
  recipe = None
  if b1.HasField('recipe') or b2.HasField('recipe'):  # pragma: no branch
    recipe = copy.deepcopy(b1.recipe)
    _merge_recipe(recipe, b2.recipe)

  b1.MergeFrom(b2)
  b1.dimensions[:] = format_dimensions(dims)
  b1.swarming_tags[:] = sorted(set(b1.swarming_tags))

  caches = [t[1] for t in sorted({c.name: c for c in b1.caches}.iteritems())]
  del b1.caches[:]
  b1.caches.extend(caches)

  if recipe:  # pragma: no branch
    b1.recipe.CopyFrom(recipe)


def flatten_builder(builder, defaults, mixins):
  """Inlines defaults and mixins into the builder.

  Applies defaults, then mixins and then reapplies values defined in |builder|.
  Flattenes defaults and referenced mixins recursively.

  This operation is NOT idempotent if defaults!=None.

  Args:
    builder (project_config_pb2.Builder): the builder to flatten.
    defaults (project_config_pb2.Builder): builder defaults.
      May use mixins.
    mixins ({str: project_config_pb2.Builder} dict): a map of mixin names
      that can be inlined. All referenced mixins must be in this dict.
      Applied after defaults.
  """
  if not defaults and not builder.mixins:
    return
  orig_mixins = builder.mixins
  builder.ClearField('mixins')
  orig_without_mixins = copy.deepcopy(builder)
  if defaults:
    flatten_builder(defaults, None, mixins)
    merge_builder(builder, defaults)
  for m in orig_mixins:
    flatten_builder(mixins[m], None, mixins)
    merge_builder(builder, mixins[m])
  merge_builder(builder, orig_without_mixins)


## Private code.


def _merge_recipe(r1, r2):
  """Merges Recipe message r2 into r1.

  Expects messages to be valid.

  All properties are converted to properties_j.
  """
  props = read_properties(r1)
  props.update(read_properties(r2))

  r1.MergeFrom(r2)
  r1.properties[:] = []
  r1.properties_j[:] = [
      '%s:%s' % (k, json.dumps(v))
      for k, v in sorted(props.iteritems())
      if v is not None
  ]
