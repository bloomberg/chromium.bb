#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around chrome.

Replaces all the child processes (renderer, GPU, plugins and utility) with the
IPC fuzzer. The fuzzer will then play back a specified testcase.

Depends on ipc_fuzzer being available on the same directory as chrome.
"""

import os
import platform
import subprocess
import sys

def main():
  if len(sys.argv) <= 1:
    print 'Usage: play_testcase.py [chrome_flag...] testcase'
    return 1

  script_path = os.path.realpath(__file__)
  ipc_fuzzer_dir = os.path.dirname(script_path)
  out_dir = os.path.abspath(os.path.join(ipc_fuzzer_dir, os.pardir,
                            os.pardir, 'out'));
  build_dir = ''
  chrome_path = ''
  chrome_binary = 'chrome'

  for build in ['Debug', 'Release']:
    try_build = os.path.join(out_dir, build)
    try_chrome = os.path.join(try_build, chrome_binary)
    if os.path.exists(try_chrome):
      build_dir = try_build
      chrome_path = try_chrome

  if not chrome_path:
    print 'chrome executable not found.'
    return 1

  fuzzer_path = os.path.join(build_dir, 'ipc_fuzzer_replay')
  if not os.path.exists(fuzzer_path):
    print fuzzer_path + ' not found.'
    print ('Please use enable_ipc_fuzzer=1 GYP define and '
          'build ipc_fuzzer target.')
    return 1

  prefixes = {
    '--renderer-cmd-prefix',
    '--gpu-launcher',
    '--plugin-launcher',
    '--ppapi-plugin-launcher',
    '--utility-cmd-prefix',
  }

  args = [
    chrome_path,
    '--ipc-fuzzer-testcase=' + sys.argv[-1],
    '--no-sandbox',
    '--disable-kill-after-bad-ipc',
  ]

  launchers = {}
  for prefix in prefixes:
    launchers[prefix] = fuzzer_path

  for arg in sys.argv[1:-1]:
    if arg.find('=') != -1:
      switch, value = arg.split('=', 1)
      if switch in prefixes:
        launchers[switch] = value + ' ' + launchers[switch]
        continue
    args.append(arg)

  for switch, value in launchers.items():
    args.append(switch + '=' + value)

  print args

  return subprocess.call(args)


if __name__ == "__main__":
  sys.exit(main())
