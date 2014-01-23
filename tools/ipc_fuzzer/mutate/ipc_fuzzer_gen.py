#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generational ClusterFuzz fuzzer. It generates IPC messages using
GenerateTraits. Support of GenerateTraits for different types will be gradually
added.
"""

import argparse
import os
import random
import string
import subprocess
import sys
import tempfile
import time

# Number of IPC messages per ipcdump
NUM_IPC_MESSAGES = 1500

def random_id(size=16, chars=string.ascii_lowercase):
  return ''.join(random.choice(chars) for x in range(size))

def random_ipcdump_path(ipcdump_dir):
  return os.path.join(ipcdump_dir, 'fuzz-' + random_id() + '.ipcdump')

class GenerationalFuzzer:
  def parse_cf_args(self):
    parser = argparse.ArgumentParser()
    parser.add_argument('--input_dir')
    parser.add_argument('--output_dir')
    parser.add_argument('--no_of_files', type=int)
    self.args = args = parser.parse_args();
    if not args.input_dir or not args.output_dir or not args.no_of_files:
      parser.print_help()
      sys.exit(1)

  def get_paths(self):
    app_path_key = 'APP_PATH'
    self.util_binary = 'ipc_message_util'
    self.generate_binary = 'ipc_fuzzer_generate'

    if app_path_key not in os.environ:
      sys.exit('Env var %s should be set to chrome path' % app_path_key)
    chrome_path = os.environ[app_path_key]
    out_dir = os.path.dirname(chrome_path)
    self.util_path = os.path.join(out_dir, self.util_binary)
    self.generate_path = os.path.join(out_dir, self.generate_binary)

  def generate_ipcdump(self):
    generated_ipcdump = random_ipcdump_path(self.args.output_dir)
    cmd = [self.generate_path,
           '--count=' + str(NUM_IPC_MESSAGES),
           generated_ipcdump]
    if subprocess.call(cmd):
      sys.exit('%s failed' % self.generate_binary)

  def main(self):
    self.parse_cf_args()
    self.get_paths()
    for i in xrange(self.args.no_of_files):
      self.generate_ipcdump()
    return 0

if __name__ == "__main__":
  fuzzer = GenerationalFuzzer()
  sys.exit(fuzzer.main())
