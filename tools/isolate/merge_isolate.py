#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Merges multiple OS-specific gyp dependency lists into one that works on all
of them.


The logic is relatively simple. Takes the current conditions, add more
condition, find the strict subset. Done.
"""

import copy
import logging
import optparse
import re
import sys

from isolate_common import pretty_print, KEY_TRACKED, KEY_UNTRACKED


def union(lhs, rhs):
  """Merges two compatible datastructures composed of dict/list/set."""
  assert lhs is not None or rhs is not None
  if lhs is None:
    return copy.deepcopy(rhs)
  if rhs is None:
    return copy.deepcopy(lhs)
  assert type(lhs) == type(rhs), (lhs, rhs)
  if hasattr(lhs, 'union'):
    # Includes set, OSSettings and Configs.
    return lhs.union(rhs)
  if isinstance(lhs, dict):
    return dict((k, union(lhs.get(k), rhs.get(k))) for k in set(lhs).union(rhs))
  elif isinstance(lhs, list):
    # Do not go inside the list.
    return lhs + rhs
  assert False, type(lhs)


def extract_comment(content):
  """Extracts file level comment."""
  out = []
  for line in content.splitlines(True):
    if line.startswith('#'):
      out.append(line)
    else:
      break
  return ''.join(out)


def eval_content(content):
  """Evaluates a GYP file and return the value defined in it."""
  globs = {'__builtins__': None}
  locs = {}
  value = eval(content, globs, locs)
  assert locs == {}, locs
  assert globs == {'__builtins__': None}, globs
  return value


def verify_variables(variables):
  """Verifies the |variables| dictionary is in the expected format."""
  VALID_VARIABLES = [
    KEY_TRACKED,
    KEY_UNTRACKED,
    'command',
    'read_only',
  ]
  assert isinstance(variables, dict), variables
  assert set(VALID_VARIABLES).issuperset(set(variables)), variables.keys()
  for name, value in variables.iteritems():
    if name == 'read_only':
      assert value in (True, False, None), value
    else:
      assert isinstance(value, list), value
      assert all(isinstance(i, basestring) for i in value), value


def verify_condition(condition):
  """Verifies the |condition| dictionary is in the expected format."""
  VALID_INSIDE_CONDITION = ['variables']
  assert isinstance(condition, list), condition
  assert 2 <= len(condition) <= 3, condition
  assert re.match(r'OS==\"([a-z]+)\"', condition[0]), condition[0]
  for c in condition[1:]:
    assert isinstance(c, dict), c
    assert set(VALID_INSIDE_CONDITION).issuperset(set(c)), c.keys()
    verify_variables(c.get('variables', {}))


def verify_root(value):
  VALID_ROOTS = ['variables', 'conditions']
  assert isinstance(value, dict), value
  assert set(VALID_ROOTS).issuperset(set(value)), value.keys()
  verify_variables(value.get('variables', {}))

  conditions = value.get('conditions', [])
  assert isinstance(conditions, list), conditions
  for condition in conditions:
    verify_condition(condition)


class OSSettings(object):
  """Represents the dependencies for an OS. The structure is immutable."""
  def __init__(self, name, values):
    self.name = name
    verify_variables(values)
    self.tracked = sorted(values.get(KEY_TRACKED, []))
    self.untracked = sorted(values.get(KEY_UNTRACKED, []))
    self.command = values.get('command', [])[:]
    self.read_only = values.get('read_only')

  def union(self, rhs):
    assert self.name == rhs.name
    assert not (self.command and rhs.command)
    var = {
      KEY_TRACKED: sorted(self.tracked + rhs.tracked),
      KEY_UNTRACKED: sorted(self.untracked + rhs.untracked),
      'command': self.command or rhs.command,
      'read_only': rhs.read_only if self.read_only is None else self.read_only,
    }
    return OSSettings(self.name, var)

  def flatten(self):
    out = {}
    if self.command:
      out['command'] = self.command
    if self.tracked:
      out[KEY_TRACKED] = self.tracked
    if self.untracked:
      out[KEY_UNTRACKED] = self.untracked
    if self.read_only is not None:
      out['read_only'] = self.read_only
    return out


class Configs(object):
  """Represents all the OS-specific configurations.

  The self.per_os[None] member contains all the 'else' clauses plus the default
  values. It is not included in the flatten() result.
  """
  def __init__(self, oses, file_comment):
    self.file_comment = file_comment
    self.per_os = {
        None: OSSettings(None, {}),
    }
    self.per_os.update(dict((name, OSSettings(name, {})) for name in oses))

  def union(self, rhs):
    items = list(set(self.per_os.keys() + rhs.per_os.keys()))
    # Takes the first file comment, prefering lhs.
    out = Configs(items, self.file_comment or rhs.file_comment)
    for key in items:
      out.per_os[key] = union(self.per_os.get(key), rhs.per_os.get(key))
    return out

  def add_globals(self, values):
    for key in self.per_os:
      self.per_os[key] = self.per_os[key].union(OSSettings(key, values))

  def add_values(self, for_os, values):
    self.per_os[for_os] = self.per_os[for_os].union(OSSettings(for_os, values))

  def add_negative_values(self, for_os, values):
    """Includes the variables to all OSes except |for_os|.

    This includes 'None' so unknown OSes gets it too.
    """
    for key in self.per_os:
      if key != for_os:
        self.per_os[key] = self.per_os[key].union(OSSettings(key, values))

  def flatten(self):
    """Returns a flat dictionary representation of the configuration.

    Skips None pseudo-OS.
    """
    return dict(
        (k, v.flatten()) for k, v in self.per_os.iteritems() if k is not None)


def invert_map(variables):
  """Converts a dict(OS, dict(deptype, list(dependencies)) to a flattened view.

  Returns a tuple of:
    1. dict(deptype, dict(dependency, set(OSes)) for easier processing.
    2. All the OSes found as a set.
  """
  KEYS = (
    KEY_TRACKED,
    KEY_UNTRACKED,
    'command',
    'read_only',
  )
  out = dict((key, {}) for key in KEYS)
  for os_name, values in variables.iteritems():
    for key in (KEY_TRACKED, KEY_UNTRACKED):
      for item in values.get(key, []):
        out[key].setdefault(item, set()).add(os_name)

    # command needs special handling.
    command = tuple(values.get('command', []))
    out['command'].setdefault(command, set()).add(os_name)

    # read_only needs special handling.
    out['read_only'].setdefault(values.get('read_only'), set()).add(os_name)
  return out, set(variables)


def reduce_inputs(values, oses):
  """Reduces the invert_map() output to the strictest minimum list.

  1. Construct the inverse map first.
  2. Look at each individual file and directory, map where they are used and
     reconstruct the inverse dictionary.
  3. Do not convert back to negative if only 2 OSes were merged.

  Returns a tuple of:
    1. the minimized dictionary
    2. oses passed through as-is.
  """
  KEYS = (
    KEY_TRACKED,
    KEY_UNTRACKED,
    'command',
    'read_only',
  )
  out = dict((key, {}) for key in KEYS)
  assert all(oses), oses
  if len(oses) > 2:
    for key in KEYS:
      for item, item_oses in values.get(key, {}).iteritems():
        # Converts all oses.difference('foo') to '!foo'.
        assert all(item_oses), item_oses
        missing = oses.difference(item_oses)
        if len(missing) == 1:
          # Replace it with a negative.
          out[key][item] = set(['!' + tuple(missing)[0]])
        elif not missing:
          out[key][item] = set([None])
        else:
          out[key][item] = set(item_oses)
  else:
    for key in KEYS:
      for item, item_oses in values.get(key, {}).iteritems():
        # Converts all oses.difference('foo') to '!foo'.
        assert None not in item_oses, item_oses
        out[key][item] = set(item_oses)
  return out, oses


def convert_map_to_gyp(values, oses):
  """Regenerates back a gyp-like configuration dict from files and dirs
  mappings generated from reduce_inputs().
  """
  # First, inverse the mapping to make it dict first.
  config = {}
  for key in values:
    for item, oses in values[key].iteritems():
      if item is None:
        # For read_only default.
        continue
      for cond_os in oses:
        cond_key = None if cond_os is None else cond_os.lstrip('!')
        # Insert the if/else dicts.
        condition_values = config.setdefault(cond_key, [{}, {}])
        # If condition is negative, use index 1, else use index 0.
        cond_value = condition_values[int((cond_os or '').startswith('!'))]
        variables = cond_value.setdefault('variables', {})

        if item in (True, False):
          # One-off for read_only.
          variables[key] = item
        else:
          if isinstance(item, tuple) and item:
            # One-off for command.
            # Do not merge lists and do not sort!
            # Note that item is a tuple.
            assert key not in variables
            variables[key] = list(item)
          elif item:
            # The list of items (files or dirs). Append the new item and keep
            # the list sorted.
            l = variables.setdefault(key, [])
            l.append(item)
            l.sort()

  out = {}
  for o in sorted(config):
    d = config[o]
    if o is None:
      assert not d[1]
      out = union(out, d[0])
    else:
      c = out.setdefault('conditions', [])
      if d[1]:
        c.append(['OS=="%s"' % o] + d)
      else:
        c.append(['OS=="%s"' % o] + d[0:1])
  return out


def load_gyp(value, file_comment):
  """Parses one gyp skeleton and returns a Configs() instance.

  |value| is the loaded dictionary that was defined in the gyp file.

  The expected format is strict, anything diverting from the format below will
  throw an assert:
  {
    'variables': {
      'command': [
        ...
      ],
      'isolate_dependency_tracked': [
        ...
      ],
      'isolate_dependency_untracked': [
        ...
      ],
      'read_only': False,
    },
    'conditions': [
      ['OS=="<os>"', {
        'variables': {
          ...
        },
      }, {  # else
        'variables': {
          ...
        },
      }],
      ...
    ],
  }
  """
  verify_root(value)

  # Scan to get the list of OSes.
  conditions = value.get('conditions', [])
  oses = set(re.match(r'OS==\"([a-z]+)\"', c[0]).group(1) for c in conditions)
  configs = Configs(oses, file_comment)

  # Global level variables.
  configs.add_globals(value.get('variables', {}))

  # OS specific variables.
  for condition in conditions:
    condition_os = re.match(r'OS==\"([a-z]+)\"', condition[0]).group(1)
    configs.add_values(condition_os, condition[1].get('variables', {}))
    if len(condition) > 2:
      configs.add_negative_values(
          condition_os, condition[2].get('variables', {}))
  return configs


def load_gyps(items):
  """Parses each gyp file and returns the merged results.

  It only loads what load_gyp() can process.

  Return values:
    files: dict(filename, set(OS where this filename is a dependency))
    dirs:  dict(dirame, set(OS where this dirname is a dependency))
    oses:  set(all the OSes referenced)
    """
  configs = Configs([], None)
  for item in items:
    logging.debug('loading %s' % item)
    with open(item, 'r') as f:
      content = f.read()
    new_config = load_gyp(eval_content(content), extract_comment(content))
    logging.debug('has OSes: %s' % ','.join(k for k in new_config.per_os if k))
    configs = union(configs, new_config)
  logging.debug('Total OSes: %s' % ','.join(k for k in configs.per_os if k))
  return configs


def print_all(comment, data, stream):
  """Prints a complete .isolate file and its top-level file comment into a
  stream.
  """
  if comment:
    stream.write(comment)
  pretty_print(data, stream)


def main(args=None):
  parser = optparse.OptionParser(
      usage='%prog <options> [file1] [file2] ...')
  parser.add_option(
      '-v', '--verbose', action='count', default=0, help='Use multiple times')
  parser.add_option(
      '-o', '--output', help='Output to file instead of stdout')

  options, args = parser.parse_args(args)
  level = [logging.ERROR, logging.INFO, logging.DEBUG][min(2, options.verbose)]
  logging.basicConfig(
        level=level,
        format='%(levelname)5s %(module)15s(%(lineno)3d):%(message)s')

  configs = load_gyps(args)
  data = convert_map_to_gyp(*reduce_inputs(*invert_map(configs.flatten())))
  if options.output:
    with open(options.output, 'w') as f:
      print_all(configs.file_comment, data, f)
  else:
    print_all(configs.file_comment, data, sys.stdout)
  return 0


if __name__ == '__main__':
  sys.exit(main())
