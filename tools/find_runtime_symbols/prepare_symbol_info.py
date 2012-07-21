#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile

from parse_proc_maps import parse_proc_maps
from util import executable_condition


def prepare_symbol_info(maps_path, output_dir_path=None, loglevel=logging.WARN):
  log = logging.getLogger('prepare_symbol_info')
  log.setLevel(loglevel)
  handler = logging.StreamHandler()
  handler.setLevel(loglevel)
  formatter = logging.Formatter('%(message)s')
  handler.setFormatter(formatter)
  log.addHandler(handler)

  if not output_dir_path:
    matched = re.match('^(.*)\.maps$', os.path.basename(maps_path))
    if matched:
      output_dir_path = matched.group(1) + '.pre'
  if not output_dir_path:
    matched = re.match('^/proc/(.*)/maps$', os.path.realpath(maps_path))
    if matched:
      output_dir_path = matched.group(1) + '.pre'
  if not output_dir_path:
    output_dir_prefix = os.path.basename(maps_path) + '.pre'
  # TODO(dmikurube): Find another candidate for output_dir_path.

  log.info('Data for profiling will be collected in "%s".' % output_dir_path)
  output_dir_path_exists = False
  if os.path.exists(output_dir_path):
    if os.path.isdir(output_dir_path) and not os.listdir(output_dir_path):
      log.warn('Using an empty directory existing at "%s".' % output_dir_path)
    else:
      log.warn('A file or a directory exists at "%s".' % output_dir_path)
      output_dir_path_exists = True
  else:
    log.info('Creating a new directory at "%s".' % output_dir_path)
    os.mkdir(output_dir_path)

  if output_dir_path_exists:
    return 1

  shutil.copyfile(maps_path, os.path.join(output_dir_path, 'maps'))

  with open(maps_path, mode='r') as f:
    maps = parse_proc_maps(f)

  log.debug('Listing up symbols.')
  nm_files = {}
  for entry in maps.iter(executable_condition):
    log.debug('  %016x-%016x +%06x %s' % (
        entry.begin, entry.end, entry.offset, entry.name))
    with tempfile.NamedTemporaryFile(
        prefix=os.path.basename(entry.name) + '.',
        suffix='.nm', delete=False, mode='w', dir=output_dir_path) as f:
      nm_filename = os.path.realpath(f.name)
      nm_succeeded = False
      cppfilt_succeeded = False
      p_nm = subprocess.Popen(
          'nm -n --format bsd %s' % entry.name, shell=True,
          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
      p_cppfilt = subprocess.Popen(
          'c++filt', shell=True,
          stdin=p_nm.stdout, stdout=f, stderr=subprocess.PIPE)

      if p_nm.wait() == 0:
        nm_succeeded = True
      for line in p_nm.stderr:
        log.debug(line.rstrip())
      if p_cppfilt.wait() == 0:
        cppfilt_succeeded = True
      for line in p_cppfilt.stderr:
        log.debug(line.rstrip())

    if nm_succeeded and cppfilt_succeeded:
      nm_files[entry.name] = {
        'file': os.path.basename(nm_filename),
        'format': 'bsd',
        'mangled': False}
    else:
      os.remove(nm_filename)

  with open(os.path.join(output_dir_path, 'nm.json'), 'w') as f:
    json.dump(nm_files, f, indent=2, sort_keys=True)

  log.info('Collected symbol information at "%s".' % output_dir_path)
  return 0


def main():
  if not sys.platform.startswith('linux'):
    sys.stderr.write('This script work only on Linux.')
    return 1

  if len(sys.argv) < 2:
    sys.stderr.write("""Usage:
%s /path/to/maps [/path/to/output_data_dir/]
""" % sys.argv[0])
    return 1
  elif len(sys.argv) == 2:
    sys.exit(prepare_symbol_info(sys.argv[1], loglevel=logging.DEBUG))
  else:
    sys.exit(prepare_symbol_info(sys.argv[1], sys.argv[2],
                                 loglevel=logging.INFO))
  return 0


if __name__ == '__main__':
  sys.exit(main())
