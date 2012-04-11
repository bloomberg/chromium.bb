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

import trace_inputs


def union(lhs, rhs):
  """Merges two compatible datastructures composed of dict/list/set."""
  assert lhs is not None or rhs is not None
  if lhs is None:
    return copy.deepcopy(rhs)
  if rhs is None:
    return copy.deepcopy(lhs)
  assert type(lhs) == type(rhs), (lhs, rhs)
  if isinstance(lhs, dict):
    return dict((k, union(lhs.get(k), rhs.get(k))) for k in set(lhs).union(rhs))
  elif isinstance(lhs, set):
    # Do not go inside the set.
    return lhs.union(rhs)
  elif isinstance(lhs, list):
    # Do not go inside the list.
    return lhs + rhs
  assert False, type(lhs)


def process_variables(for_os, variables):
  """Extracts files and dirs from the |variables| dict.

  Returns a list of exactly two items. Each item is a dict that maps a string
  to a set (of strings).

  In the first item, the keys are file names, and the values are sets of OS
  names, like "win" or "mac". In the second item, the keys are directory names,
  and the values are sets of OS names too.
  """
  VALID_VARIABLES = ['isolate_files', 'isolate_dirs']

  # Verify strictness.
  assert isinstance(variables, dict), variables
  assert set(VALID_VARIABLES).issuperset(set(variables)), variables.keys()
  for items in variables.itervalues():
    assert isinstance(items, list), items
    assert all(isinstance(i, basestring) for i in items), items

  # Returns [files, dirs]
  return [
    dict((name, set([for_os])) for name in variables.get(var, []))
    for var in VALID_VARIABLES
  ]


def eval_content(content):
  """Evaluates a GYP file and return the value defined in it."""
  globs = {'__builtins__': None}
  locs = {}
  value = eval(content, globs, locs)
  assert locs == {}, locs
  assert globs == {'__builtins__': None}, globs
  return value


def _process_inner(for_os, inner, old_files, old_dirs, old_os):
  """Processes the variables inside a condition.

  Only meant to be called by parse_gyp_dict().

  Args:
  - for_os: OS where the references are tracked for.
  - inner: Inner dictionary to process.
  - old_files: Previous list of files to union with.
  - old_dirs: Previous list of directories to union with.
  - old_os: Previous list of OSes referenced to union with.

  Returns:
  - A tuple of (files, dirs, os) where each list is a union of the new
    dependencies found for this OS, as referenced by for_os, and the previous
    list.
  """
  assert isinstance(inner, dict), inner
  assert set(['variables']).issuperset(set(inner)), inner.keys()
  new_files, new_dirs = process_variables(for_os, inner.get('variables', {}))
  if new_files or new_dirs:
    old_os = old_os.union([for_os.lstrip('!')])
  return union(old_files, new_files), union(old_dirs, new_dirs), old_os


def parse_gyp_dict(value):
  """Parses a gyp dict as returned by eval_content().

  |value| is the loaded dictionary that was defined in the gyp file.

  Returns a 3-tuple, where the first two items are the same as the items
  returned by process_variable() in the same order, and the last item is a set
  of strings of all OSs seen in the input dict.

  The expected format is strict, anything diverting from the format below will
  fail:
  {
    'variables': {
      'isolate_files': [
        ...
      ],
      'isolate_dirs: [
        ...
      ],
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
  assert isinstance(value, dict), value
  VALID_ROOTS = ['variables', 'conditions']
  assert set(VALID_ROOTS).issuperset(set(value)), value.keys()

  # Global level variables.
  oses = set()
  files, dirs = process_variables(None, value.get('variables', {}))

  # OS specific variables.
  conditions = value.get('conditions', [])
  assert isinstance(conditions, list), conditions
  for condition in conditions:
    assert isinstance(condition, list), condition
    assert 2 <= len(condition) <= 3, condition
    m = re.match(r'OS==\"([a-z]+)\"', condition[0])
    assert m, condition[0]
    condition_os = m.group(1)

    files, dirs, oses = _process_inner(
        condition_os, condition[1], files, dirs, oses)

    if len(condition) == 3:
      files, dirs, oses = _process_inner(
          '!' + condition_os, condition[2], files, dirs, oses)

  # TODO(maruel): _expand_negative() should be called here, because otherwise
  # the OSes the negative condition represents is lost once the gyps are merged.
  # This cause an invalid expansion in reduce_inputs() call.
  return files, dirs, oses


def parse_gyp_dicts(gyps):
  """Parses each gyp file and returns the merged results.

  It only loads what parse_gyp_dict() can process.

  Return values:
    files: dict(filename, set(OS where this filename is a dependency))
    dirs:  dict(dirame, set(OS where this dirname is a dependency))
    oses:  set(all the OSes referenced)
    """
  files = {}
  dirs = {}
  oses = set()
  for gyp in gyps:
    with open(gyp, 'rb') as gyp_file:
      content = gyp_file.read()
    gyp_files, gyp_dirs, gyp_oses = parse_gyp_dict(eval_content(content))
    files = union(gyp_files, files)
    dirs = union(gyp_dirs, dirs)
    oses |= gyp_oses
  return files, dirs, oses


def _expand_negative(items, oses):
  """Converts all '!foo' value in the set by oses.difference('foo')."""
  assert None not in oses and len(oses) >= 2, oses
  for name in items:
    if None in items[name]:
      # Shortcut any item having None in their set. An item listed in None means
      # the item is a dependency on all OSes. As such, there is no need to list
      # any OS.
      items[name] = set([None])
      continue
    for neg in [o for o in items[name] if o.startswith('!')]:
      # Replace it with the inverse.
      items[name] = items[name].union(oses.difference([neg[1:]]))
      items[name].remove(neg)
    if items[name] == oses:
      items[name] = set([None])


def _compact_negative(items, oses):
  """Converts all oses.difference('foo') to '!foo'.

  It is doing the reverse of _expand_negative().
  """
  assert None not in oses and len(oses) >= 3, oses
  for name in items:
    missing = oses.difference(items[name])
    if len(missing) == 1:
      # Replace it with a negative.
      items[name] = set(['!' + tuple(missing)[0]])


def reduce_inputs(files, dirs, oses):
  """Reduces the variables to their strictest minimum."""
  # Construct the inverse map first.
  # Look at each individual file and directory, map where they are used and
  # reconstruct the inverse dictionary.
  # First, expands all '!' builders into the reverse.
  # TODO(maruel): This is too late to call _expand_negative(). The exact list
  # negative OSes condition it represents is lost at that point.
  _expand_negative(files, oses)
  _expand_negative(dirs, oses)

  # Do not convert back to negative if only 2 OSes were merged. It is easier to
  # read this way.
  if len(oses) > 2:
    _compact_negative(files, oses)
    _compact_negative(dirs, oses)

  return files, dirs


def convert_to_gyp(files, dirs):
  """Regenerates back a gyp-like configuration dict from files and dirs
  mappings.

  Sort the lists.
  """
  # First, inverse the mapping to make it dict first.
  config = {}
  def to_cond(items, name):
    for item, oses in items.iteritems():
      for cond_os in oses:
        condition_values = config.setdefault(
            None if cond_os is None else cond_os.lstrip('!'),
            [{}, {}])
        # If condition is negative, use index 1, else use index 0.
        condition_value = condition_values[int((cond_os or '').startswith('!'))]
        # The list of items (files or dirs). Append the new item and keep the
        # list sorted.
        l = condition_value.setdefault('variables', {}).setdefault(name, [])
        l.append(item)
        l.sort()

  to_cond(files, 'isolate_files')
  to_cond(dirs, 'isolate_dirs')

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


def main():
  parser = optparse.OptionParser(
      usage='%prog <options> [file1] [file2] ...')
  parser.add_option(
      '-v', '--verbose', action='count', default=0, help='Use multiple times')

  options, args = parser.parse_args()
  level = [logging.ERROR, logging.INFO, logging.DEBUG][min(2, options.verbose)]
  logging.basicConfig(
        level=level,
        format='%(levelname)5s %(module)15s(%(lineno)3d):%(message)s')

  trace_inputs.pretty_print(
      convert_to_gyp(*reduce_inputs(*parse_gyp_dicts(args))),
      sys.stdout)
  return 0


if __name__ == '__main__':
  sys.exit(main())
