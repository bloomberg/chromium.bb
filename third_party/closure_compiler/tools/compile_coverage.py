#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from ast import literal_eval
import os


# High priority code to compile.
_NEED_TO_COMPILE = [
  'chrome/browser/resources',
  'chrome/browser/ui/webui',
  'ui/webui/resources/js',
]


# Code that we'd eventually like to compile.
_WANT_TO_COMPILE = [
  'chrome/renderer/resources',
  'chrome/test/data',
  'content/renderer/resources',
  'content/test/data',
  'extensions/renderer',
  'extensions/test/data',
  'remoting',
  'ui/file_manager',
  'ui/keyboard',
]


def main():
  here = os.path.dirname(__file__)
  src_root = os.path.join(here, '..', '..', '..')

  line_cache = {}

  def js_files_in_dir(js_dir):
    found_files = set()
    for root, dirs, files in os.walk(js_dir):
      js = filter(lambda f: f.endswith('.js'), files)
      found_files.update([os.path.abspath(os.path.join(root, f)) for f in js])
    return found_files

  def num_lines(f):
    if f not in line_cache:
      line_cache[f] = len(open(f, 'r').read().splitlines())
    return line_cache[f]

  # All the files that are already compiled.
  compiled = set()

  closure_dir = os.path.join(here, '..')
  root_gyp = os.path.join(closure_dir, 'compiled_resources.gyp')
  root_contents = open(root_gyp, 'r').read()
  gyp_files = literal_eval(root_contents)['targets'][0]['dependencies']

  for g in gyp_files:
    src_to_closure = os.path.relpath(src_root, closure_dir)
    rel_file = os.path.relpath(g.replace(':*', ''), src_to_closure)
    abs_file = os.path.abspath(rel_file)
    targets = literal_eval(open(abs_file, 'r').read())['targets']

    for target in targets:
      abs_dir = os.path.dirname(abs_file)
      target_file = os.path.join(abs_dir, target['target_name'] + '.js')
      compiled.add(target_file)

      if 'variables' in target and 'depends' in target['variables']:
        depends = target['variables']['depends']
        rel_depends = [os.path.join(abs_dir, d) for d in depends]
        compiled.update([os.path.abspath(d) for d in rel_depends])

  compiled_lines = sum(map(num_lines, compiled))
  print 'compiled: %d files, %d lines' % (len(compiled), compiled_lines)

  # Find and calculate the line count of all .js files in the wanted or needed
  # resource directories.
  files = set()

  for n in _NEED_TO_COMPILE:
    files.update(js_files_in_dir(os.path.join(src_root, n)))

  need_lines = sum(map(num_lines, files))
  print 'need: %d files, %d lines' % (len(files), need_lines)

  need_done = float(compiled_lines) / need_lines * 100
  print '%.2f%% done with the code we need to compile' % need_done

  for w in _WANT_TO_COMPILE:
    files.update(js_files_in_dir(os.path.join(src_root, w)))

  want_lines = sum(map(num_lines, files))
  print 'want: %d files, %d lines' % (len(files), want_lines)

  want_done = float(compiled_lines) / want_lines * 100
  print '%.2f%% done with the code we want to compile' % want_done


if __name__ == '__main__':
  main()
