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

from proc_maps import ProcMaps


def _dump_command_result(command, output_dir_path, basename, suffix, log):
  handle_out, filename_out = tempfile.mkstemp(
      suffix=suffix, prefix=basename + '.', dir=output_dir_path)
  handle_err, filename_err = tempfile.mkstemp(
      suffix=suffix + '.err', prefix=basename + '.', dir=output_dir_path)
  error = False
  try:
    subprocess.check_call(
        command, stdout=handle_out, stderr=handle_err, shell=True)
  except:
    error = True
  finally:
    os.close(handle_err)
    os.close(handle_out)

  if os.path.exists(filename_err):
    if log.getEffectiveLevel() <= logging.DEBUG:
      with open(filename_err, 'r') as f:
        for line in f:
          log.debug(line.rstrip())
    os.remove(filename_err)

  if os.path.exists(filename_out) and (
      os.path.getsize(filename_out) == 0 or error):
    os.remove(filename_out)
    return None

  if not os.path.exists(filename_out):
    return None

  return filename_out


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
    maps = ProcMaps.load(f)

  log.debug('Listing up symbols.')
  files = {}
  for entry in maps.iter(ProcMaps.executable):
    log.debug('  %016x-%016x +%06x %s' % (
        entry.begin, entry.end, entry.offset, entry.name))
    nm_filename = _dump_command_result(
        'nm -n --format bsd %s | c++filt' % entry.name,
        output_dir_path, os.path.basename(entry.name), '.nm', log)
    if not nm_filename:
      continue
    readelf_e_filename = _dump_command_result(
        'readelf -eW %s' % entry.name,
        output_dir_path, os.path.basename(entry.name), '.readelf-e', log)
    if not readelf_e_filename:
      continue

    files[entry.name] = {}
    files[entry.name]['nm'] = {
        'file': os.path.basename(nm_filename),
        'format': 'bsd',
        'mangled': False}
    files[entry.name]['readelf-e'] = {
        'file': os.path.basename(readelf_e_filename)}

  with open(os.path.join(output_dir_path, 'files.json'), 'w') as f:
    json.dump(files, f, indent=2, sort_keys=True)

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
    sys.exit(prepare_symbol_info(sys.argv[1], loglevel=logging.INFO))
  else:
    sys.exit(prepare_symbol_info(sys.argv[1], sys.argv[2],
                                 loglevel=logging.INFO))
  return 0


if __name__ == '__main__':
  sys.exit(main())
