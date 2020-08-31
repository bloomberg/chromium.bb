# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for working with FieldMask protobufs."""

from __future__ import print_function

import sys

from chromite.lib import cros_logging as logging


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


def _MergeDictWithPathParts(path_parts, source, destination):
  """Merges source into destination based on path_parts.

  Args:
    path_parts (list[str]): A list with each part of a single FieldMask path as
        an element. E.g. path 'a.b.c' would be list ['a', 'b', 'c']
    source (dict): The source dict.
    destination (dict): The destination message to be merged into.
  """
  assert path_parts
  cur_part = path_parts[0]

  if not cur_part:
    raise ValueError('Field cannot be empty string')

  if cur_part not in source:
    # There are cases when a field is specified that is not part of the JSON
    # source. For example, ChromeOS Config payloads are filtered by specifying
    # a field mask that will be applied to each device config, and a field might
    # only be set in some device configs. I.e. there could be a payload
    #
    # {
    #   "chromeos": {
    #     "configs": [
    #       {"bluetooth": {...}, "modem": {...}},
    #       {"bluetooth": {...}}
    #     ]
    #   }
    # }
    #
    # and a field mask "bluetooth,modem".
    #
    # In this case, log a warning (as this might be caused by a mistake in the
    # config) and move on.
    logging.warning('Field %s not found.', cur_part)
    return

  if len(path_parts) == 1:
    # If there is only one part of the path left, set it in the destination.
    destination[cur_part] = source[cur_part]
  else:
    # Lists are only allowed as the last part of a path string (mirrors
    # behavior for standard protos).
    if isinstance(source[cur_part], list):
      raise ValueError('Field %s is a list and cannot have sub-fields' %
                       cur_part)

    # Recursively call with the remaining path parts.
    if cur_part not in destination:
      destination[cur_part] = {}
    _MergeDictWithPathParts(path_parts[1:], source[cur_part],
                            destination[cur_part])


def CreateFilteredDict(field_mask, source):
  """Returns a copy of source filtered by field_mask.

  Similar to the FieldMask.MergeMessage method, but for general Python dicts,
  e.g. parsed from JSON.

  Args:
    field_mask (FieldMask): The FieldMask to apply.
    source (dict): The source dict.
  """
  destination = {}
  for path in field_mask.paths:
    _MergeDictWithPathParts(path.split('.'), source, destination)

  return destination
