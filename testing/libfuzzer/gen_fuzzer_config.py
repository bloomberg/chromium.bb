#!/usr/bin/python2
#
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate or update an existing config (.options file) for libfuzzer test.

Invoked by GN from fuzzer_test.gni.
"""

import argparse
import os
import sys


OPTIONS_FILE_TEMPLATE = '''# This is automatically generated fuzzer config.
[libfuzzer]
dict = %(dict)s
'''

def main():
  parser = argparse.ArgumentParser(description="Generate fuzzer config.")
  parser.add_argument('--config', required=True)
  parser.add_argument('--dict', required=True)
  parser.add_argument('--libfuzzer_options', required=False)
  args = parser.parse_args()

  config_path = args.config
  # Dict will be copied into build directory, use only its basename for config.
  dict_name = os.path.basename(args.dict)

  if not args.libfuzzer_options:
    # Generate .options file with initialized 'dict' option.
    with open(config_path, 'w') as options_file:
      options_file.write(OPTIONS_FILE_TEMPLATE % {'dict': dict_name})
    return

  # Append 'dict' option to an existing .options file.
  initial_config = open(args.libfuzzer_options).read()
  with open(config_path, 'w') as options_file:
    options_file.write(initial_config)
    options_file.write('\ndict = %s\n' % dict_name)


if __name__ == '__main__':
  main()
