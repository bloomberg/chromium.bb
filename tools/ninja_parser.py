#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Primitive ninja parser.

It's primary use case is to be used by isolate_driver.py. It is a standalone
tool so it can be used to verify its behavior more efficiently. It's a quirky
tool since it is designed only to gather binary dependencies.

<directory> is assumed to be based on src/, so usually it will have the form
out/Release. Any target outside this directory is ignored.

This script is assumed to be run *after* the target was built, since it looks
for the +x bit on the files and only returns the ones that exist.
"""

import logging
import os
import optparse
import sys
import time

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.dirname(TOOLS_DIR)


### Private stuff.


_HEX = frozenset('0123456789abcdef')


# Ignore any dependency with one of these extensions. In particular, we do not
# want to mark tool that would have generated these as binary dependencies,
# these are likely tools only used during the build process that are not
# necessary at runtime. The worst thing that would happen is either:
# - Mapping tools generating a source file but not necessary at run time.
# - Slower processing.
# On the other hand, being too aggressive will hide legitimate runtime
# dependencies. In particular, .a may cause an explicit dependency on a .so.
_IGNORED_EXTENSIONS = frozenset([
  '.asm', '.c', '.cc', '.cpp', '.css', '.def', '.grd', '.gypcmd', '.h',
  '.html', '.idl', '.in', '.jinja2', '.js', '.json', '.manifest', '.mm', '.o',
  '.obj', '.pak', '.pickle', '.png', '.pdb', '.prep', '.proto', '.py', '.rc',
  '.strings', '.svg', '.tmp', '.ttf', '.txt', '.xtb', '.wav',
  ])


def _load_ninja_recursively(build_dir, ninja_path, build_steps):
  """Crudely extracts all the subninja and build referenced in ninja_path.

  In particular, it ignores rule and variable declarations. The goal is to be
  performant (well, as much as python can be performant) which is currently in
  the <200ms range for a complete chromium tree. As such the code is laid out
  for performance instead of readability.
  """
  logging.debug('Loading %s', ninja_path)
  try:
    with open(os.path.join(build_dir, ninja_path), 'rb') as f:
      line = None
      merge_line = ''
      subninja = []
      for line in f:
        line = line.rstrip()
        if not line:
          continue

        if line[-1] == '$':
          # The next line needs to be merged in.
          if merge_line:
            merge_line += ' ' + line[:-1].strip(' ')
          else:
            merge_line = line[:-1].strip(' ')
          continue

        if merge_line:
          line = merge_line + ' ' + line.strip(' ')
          merge_line = ''

        statement = line[:line.find(' ')]
        if statement == 'build':
          # Save the dependency list as a raw string. Only the lines needed will
          # be processed within _recurse(). This saves a good 70ms of processing
          # time.
          build_target, dependencies = line[6:].split(': ', 1)
          # Interestingly, trying to be smart and only saving the build steps
          # with the intended extensions ('', '.stamp', '.so') slows down
          # parsing even if 90% of the build rules can be skipped.
          # On Windows, a single step may generate two target, so split items
          # accordingly. It has only been seen for .exe/.exe.pdb combos.
          for i in build_target.strip().split():
            build_steps[i] = dependencies
        elif statement == 'subninja':
          subninja.append(line[9:])
  except IOError:
    print >> sys.stderr, 'Failed to open %s' % ninja_path
    raise

  total = 1
  for rel_path in subninja:
    try:
      # Load each of the files referenced.
      # TODO(maruel): Skip the files known to not be needed. It saves an aweful
      # lot of processing time.
      total += _load_ninja_recursively(build_dir, rel_path, build_steps)
    except IOError:
      print >> sys.stderr, '... as referenced by %s' % ninja_path
      raise
  return total


def _simple_blacklist(item):
  """Returns True if an item should be analyzed."""
  return item not in ('', '|', '||')


def _using_blacklist(item):
  """Returns True if an item should be analyzed.

  Ignores many rules that are assumed to not depend on a dynamic library. If
  the assumption doesn't hold true anymore for a file format, remove it from
  this list. This is simply an optimization.
  """
  # ninja files use native path format.
  ext = os.path.splitext(item)[1]
  if ext in _IGNORED_EXTENSIONS:
    return False
  # Special case Windows, keep .dll.lib but discard .lib.
  if item.endswith('.dll.lib'):
    return True
  if ext == '.lib':
    return False
  return _simple_blacklist(item)


def _should_process(build_dir, target, build_steps, rules_seen):
  """Returns the raw dependencies if the target should be processed."""
  if target in rules_seen:
    # The rule was already seen. Since rules_seen is not scoped at the target
    # visibility, it usually simply means that multiple targets depends on the
    # same dependencies. It's fine.
    return None

  raw_dependencies = build_steps.get(target, None)
  if raw_dependencies is None:
    # There is not build step defined to generate 'target'.
    parts = target.rsplit('_', 1)
    if len(parts) == 2 and len(parts[1]) == 32 and _HEX.issuperset(parts[1]):
      # It's because it is a phony rule.
      return None

    # Kind of a hack, assume source files are always outside build_bir.
    if (target.startswith('..') and
        os.path.exists(os.path.join(build_dir, target))):
      # It's because it is a source file.
      return None

    logging.debug('Failed to find a build step to generate: %s', target)
    return None
  return raw_dependencies


def _recurse(build_dir, target, build_steps, rules_seen, blacklist):
  raw_dependencies = _should_process(build_dir, target, build_steps, rules_seen)
  rules_seen.add(target)
  if raw_dependencies is None:
    return []

  out = [target]
  # Filter out what we don't want to speed things up. This cuts off large parts
  # of the dependency tree to analyze.
  # The first item is the build rule, e.g. 'link', 'cxx', 'phony', 'stamp', etc.
  dependencies = filter(blacklist, raw_dependencies.split(' ')[1:])
  logging.debug('recurse(%s) -> %s', target, dependencies)
  for dependency in dependencies:
    out.extend(_recurse(
        build_dir, dependency, build_steps, rules_seen, blacklist))
  return out


def _find_link(build_dir, target, build_steps, rules_seen, search_for):
  raw_dependencies = _should_process(build_dir, target, build_steps, rules_seen)
  rules_seen.add(target)
  if raw_dependencies is None:
    return

  # Filter out what we don't want to speed things up. This cuts off large parts
  # of the dependency tree to analyze.
  # The first item is the build rule, e.g. 'link', 'cxx', 'phony', 'stamp', etc.
  dependencies = filter(_simple_blacklist, raw_dependencies.split(' ')[1:])
  for dependency in dependencies:
    if dependency == search_for:
      yield [dependency]
    else:
      for out in _find_link(
          build_dir, dependency, build_steps, rules_seen, search_for):
        yield [dependency] + out


### Public API.


def load_ninja(build_dir):
  """Loads build.ninja and the tree of .ninja files in build_dir.

  Returns:
    dict of the target->build_step where build_step is an unprocessed str.

  TODO(maruel): This should really just be done by ninja itself, then simply
  process its output.
  """
  build_steps = {}
  total = _load_ninja_recursively(build_dir, 'build.ninja', build_steps)
  logging.info('Loaded %d ninja files, %d build steps', total, len(build_steps))
  return build_steps


def recurse(build_dir, target, build_steps, blacklist=_using_blacklist):
  """Recursively returns all the interesting dependencies for the target
  specified.
  """
  return _recurse(build_dir, target, build_steps, set(), blacklist)


def find_link(build_dir, target, build_steps, search_for):
  """Finds all the links from 'target' to 'search_for'.

  Example:
    With target='foo' and search_for='baz', if foo depends on bar which depends
    on baz, it will yield only one list, ['foo', 'bar', 'baz']. If there were
    multiple dependency links in the DAG, each one would be yield as a list.
  """
  for link in _find_link(build_dir, target, build_steps, set(), search_for):
    yield link


def post_process_deps(build_dir, dependencies):
  """Processes the dependency list with OS specific rules.

  It maps the dynamic libraries import library into their actual dynamic
  library.

  Must be run after the build is completed.
  """
  def filter_item(i):
    if i.endswith('.so.TOC'):
      # Remove only the suffix .TOC, not the .so!
      return i[:-4]
    if i.endswith('.dylib.TOC'):
      # Remove only the suffix .TOC, not the .dylib!
      return i[:-4]
    if i.endswith('.dll.lib'):
      # Remove only the suffix .lib, not the .dll!
      return i[:-4]
    return i

  # Check for execute access. This gets rid of all the phony rules.
  return [
    i for i in map(filter_item, dependencies)
    if os.access(os.path.join(build_dir, i), os.X_OK)
  ]


def main():
  parser = optparse.OptionParser(
      usage='%prog [options] <directory> <target> <search_for>',
      description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-v', '--verbose', action='count', default=0,
      help='Use twice for more info')
  options, args = parser.parse_args()

  levels = (logging.ERROR, logging.INFO, logging.DEBUG)
  logging.basicConfig(
      level=levels[min(len(levels)-1, options.verbose)],
      format='%(levelname)7s %(message)s')
  if len(args) == 2:
    build_dir, target = args
    search_for = None
  elif len(args) == 3:
    build_dir, target, search_for = args
  else:
    parser.error('Please provide a directory, a target, optionally a link')

  start = time.time()
  build_dir = os.path.abspath(build_dir)
  if not os.path.isdir(build_dir):
    parser.error('build dir must exist')
  build_steps = load_ninja(build_dir)

  if search_for:
    found_one = False
    # Find how to get to link from target.
    for rules in find_link(build_dir, target, build_steps, search_for):
      found_one = True
      print('%s -> %s' % (target, ' -> '.join(rules)))
    if not found_one:
      print('Find to find a link between %s and %s' % (target, search_for))
    end = time.time()
    logging.info('Processing took %.3fs', end-start)
  else:
    binary_deps = post_process_deps(
        build_dir, recurse(build_dir, target, build_steps))
    end = time.time()
    logging.info('Processing took %.3fs', end-start)
    print('Binary dependencies:%s' % ''.join('\n  ' + i for i in binary_deps))
  return 0


if __name__ == '__main__':
  sys.exit(main())
