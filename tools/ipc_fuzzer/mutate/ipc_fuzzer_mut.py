#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Mutational ClusterFuzz fuzzer. A pre-built corpus of ipcdump files has
to be uploaded to ClusterFuzz along with this script. As chrome is being
developed, the corpus will become out-of-date and needs to be updated.

This fuzzer will pick some ipcdumps from the corpus, concatenate them with
ipc_message_util and mutate the result with ipc_fuzzer_mutate.
"""

import argparse
import os
import random
import string
import subprocess
import sys
import tempfile
import time

# Number of ipcdumps to concatenate
NUM_IPCDUMPS = 50

def create_temp_file():
  temp_file = tempfile.NamedTemporaryFile(delete=False)
  temp_file.close()
  return temp_file.name

def random_id(size=16, chars=string.ascii_lowercase):
  return ''.join(random.choice(chars) for x in range(size))

def random_ipcdump_path(ipcdump_dir):
  return os.path.join(ipcdump_dir, 'fuzz-' + random_id() + '.ipcdump')

class MutationalFuzzer:
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
    self.mutate_binary = 'ipc_fuzzer_mutate'

    if app_path_key not in os.environ:
      sys.exit('Env var %s should be set to chrome path' % app_path_key)
    chrome_path = os.environ[app_path_key]
    out_dir = os.path.dirname(chrome_path)
    self.util_path = os.path.join(out_dir, self.util_binary)
    self.mutate_path = os.path.join(out_dir, self.mutate_binary)

  def list_corpus(self):
    input_dir = self.args.input_dir
    entries = os.listdir(input_dir)
    entries = [i for i in entries if i.endswith('.ipcdump')]
    self.corpus = [os.path.join(input_dir, entry) for entry in entries]

  def create_mutated_ipcdump(self):
    ipcdumps = ','.join(random.sample(self.corpus, NUM_IPCDUMPS))
    tmp_ipcdump = create_temp_file()
    mutated_ipcdump = random_ipcdump_path(self.args.output_dir)

    # concatenate ipcdumps -> tmp_ipcdump
    cmd = [self.util_path, ipcdumps, tmp_ipcdump]
    if subprocess.call(cmd):
      sys.exit('%s failed' % self.util_binary)

    # mutate tmp_ipcdump -> mutated_ipcdump
    cmd = [self.mutate_path, tmp_ipcdump, mutated_ipcdump]
    if subprocess.call(cmd):
      sys.exit('%s failed' % self.mutate_binary)
    os.remove(tmp_ipcdump)

  def main(self):
    self.parse_cf_args()
    self.get_paths()
    self.list_corpus()
    for i in xrange(self.args.no_of_files):
      self.create_mutated_ipcdump()
    return 0

if __name__ == "__main__":
  fuzzer = MutationalFuzzer()
  sys.exit(fuzzer.main())
