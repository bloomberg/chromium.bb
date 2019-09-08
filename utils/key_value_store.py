# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common python commands used by various build scripts."""

from __future__ import print_function

import contextlib
import errno

import six


@contextlib.contextmanager
def _Open(obj, mode='r'):
  """Convenience ctx that accepts a file path or an already open file object."""
  if isinstance(obj, six.string_types):
    with open(obj, mode=mode) as f:
      yield f
  else:
    yield obj


def LoadData(data, multiline=False, source='<data>'):
  """Turn key=value content into a dict

  Note: If you're designing a new data store, please use json rather than
  this format.  This func is designed to work with legacy/external files
  where json isn't an option.

  Only UTF-8 content is supported currently.

  Args:
    data: The data to parse.
    multiline: Allow a value enclosed by quotes to span multiple lines.
    source: Helpful string for users to diagnose source of errors.

  Returns:
    a dict of all the key=value pairs found in the file.
  """
  d = {}

  key = None
  in_quotes = None
  for raw_line in data.splitlines(True):
    line = raw_line.split('#')[0]
    if not line.strip():
      continue

    # Continue processing a multiline value.
    if multiline and in_quotes and key:
      if line.rstrip()[-1] == in_quotes:
        # Wrap up the multiline value if the line ends with a quote.
        d[key] += line.rstrip()[:-1]
        in_quotes = None
      else:
        d[key] += line
      continue

    chunks = line.split('=', 1)
    if len(chunks) != 2:
      raise ValueError('Malformed key=value file %r; line %r'
                       % (source, raw_line))
    key = chunks[0].strip()
    val = chunks[1].strip()
    if len(val) >= 2 and val[0] in '"\'' and val[0] == val[-1]:
      # Strip matching quotes on the same line.
      val = val[1:-1]
    elif val and multiline and val[0] in '"\'':
      # Unmatched quote here indicates a multiline value. Do not
      # strip the '\n' at the end of the line.
      in_quotes = val[0]
      val = chunks[1].lstrip()[1:]
    d[key] = val

  return d


def LoadFile(obj, ignore_missing=False, multiline=False):
  """Turn a key=value file into a dict

  Note: If you're designing a new data store, please use json rather than
  this format.  This func is designed to work with legacy/external files
  where json isn't an option.

  Only UTF-8 content is supported currently.

  Args:
    obj: The file to read.  Can be a path or an open file object.
    ignore_missing: If the file does not exist, return an empty dict.
    multiline: Allow a value enclosed by quotes to span multiple lines.

  Returns:
    a dict of all the key=value pairs found in the file.
  """
  try:
    with _Open(obj) as f:
      return LoadData(f.read(), multiline=multiline, source=obj)
  except EnvironmentError as e:
    if not (ignore_missing and e.errno == errno.ENOENT):
      raise

  return {}
