#!/usr/bin/python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints the total PSS attributed to Chrome's code pages in an application.

This scripts assumes a device with Monochrome, and requires root access.
For instance, to get chrome's code page memory footprint:
$ tools/android/native_lib_memory/code_pages_pss.py
    --app-package com.android.chrome
    --chrome-package com.android.chrome --verbose

To get Webview's footprint in AGSA:
$ tools/android/native_lib_memory/code_pages_pss.py
    --app-package com.google.android.googlequicksearchbox
    --chrome-package com.android.chrome --verbose
"""

import argparse
import logging
import os
import re
import sys

_SRC_PATH = os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, os.pardir)
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils


def _GetPssFromProcSmapsLinesInKb(lines, chrome_package, verbose):
  """Same as |_GetPssInKb()|, starting from the content of /proc/[pid]/smaps.

  Args:
    lines: ([str]) Content of /proc/[pid]/smaps.
    chrome_package: (str) Chrome's package name, e.g. 'com.android.chrome'.
    verbose: (bool) Verbose output.
  """
  SMAPS_ENTRY_START_RE = '^[0-9a-f]{1,16}-[0-9a-f]{1,16} '
  assert re.search(SMAPS_ENTRY_START_RE,
                   '7fffc6fbe000-7fffc6fc0000 r-xp '
                   '00000000 00:00 0                          [vdso]')
  # Various kernels have various amount of detail for each /proc/[pid]/smaps
  # entry. Detect the start of a new entry to find the length on this kernel.
  assert re.search(SMAPS_ENTRY_START_RE, lines[0])
  length = 0
  for (index, line) in enumerate(lines[1:]):
    if re.search(SMAPS_ENTRY_START_RE, line):
      length = index + 1
      break
  assert length
  pss_offset = 0
  for (index, line) in enumerate(lines[:length]):
    if line.startswith('Pss:'):
      pss_offset = index
      break
  assert pss_offset

  pss = 0
  for (index, line) in enumerate(lines):
    # Look for read-only, executable private mappings coming from Chrome's
    # APK. The APK is in the file path.
    # Example line:
    # 9d241000-9fb82000 r-xp 009a0000 fd:00 5201 [...]
    #     /data/app/com.android.chrome-G1amO4b7kPsNBi51zH8QWQ==/base.apk
    if not re.search(SMAPS_ENTRY_START_RE, line):
      continue
    if not ('r-xp' in line and chrome_package in line):
      continue
    if verbose:
        print '\n'.join(lines[index:index + length])
    pss_line = lines[index + pss_offset]
    assert pss_line.startswith('Pss:')
    assert pss_line.endswith(' kB')
    pss += int(pss_line[len('Pss:'):-2])
  return pss


def _GetPssInKb(device, pid, chrome_package, verbose):
  """Returns the PSS taken by chrome's code pages for a pid, in kB.

  Args:
    device: (device_utils.DeviceUtils) Device to get the data from.
    pid: (int) PID of the process to inspect.
    chrome_package: (str) Chrome's package name, e.g. 'com.android.chrome'.
    verbose: (bool) Verbose output. Prints relevant /proc/[pid]/smaps entries
                    if set.

  Returns:
    (int) PSS in kB from Chrome's code pages in this process.
  """
  command = ['cat', '/proc/%d/smaps' % pid]
  lines = device.RunShellCommand(command, check_return=True)
  return _GetPssFromProcSmapsLinesInKb(lines, chrome_package, verbose)


def _CreateArgumentParser():
  parser = argparse.ArgumentParser()
  parser.add_argument('--app-package', help='Application to inspect.',
                      required=True)
  parser.add_argument('--chrome-package', help='Chrome package to look for.',
                      required=True)
  parser.add_argument('--verbose', help='Verbose output.',
                      action='store_true')
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = _CreateArgumentParser()
  args = parser.parse_args()
  devices = device_utils.DeviceUtils.HealthyDevices()
  if not devices:
    logging.error('No connected devices')
    return
  device = devices[0]
  device.EnableRoot()
  processes = device.ListProcesses(args.app_package)
  logging.info('Processes:\n\t' + '\n\t'.join(p.name for p in processes))
  total_pss_kb = 0
  for process in processes:
    total_pss_kb += _GetPssInKb(device, process.pid, args.chrome_package,
                                args.verbose)
  print 'Total PSS from code pages = %dkB' % total_pss_kb


if __name__ == '__main__':
  main()
