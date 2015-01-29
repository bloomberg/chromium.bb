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

import os
import random
import subprocess
import sys
import utils

IPC_MESSAGE_UTIL_APPLICATION = 'ipc_message_util'
IPC_MUTATE_APPLICATION = 'ipc_fuzzer_mutate'
IPC_REPLAY_APPLICATION = 'ipc_fuzzer_replay'
IPCDUMP_MERGE_LIMIT = 50

class MutationalFuzzer:
  def parse_arguments(self):
    self.args = utils.parse_arguments()

  def set_application_paths(self):
    chrome_application_path = utils.get_application_path()
    chrome_application_directory = os.path.dirname(chrome_application_path)

    self.ipc_message_util_binary = utils.application_name_for_platform(
        IPC_MESSAGE_UTIL_APPLICATION)
    self.ipc_mutate_binary = utils.application_name_for_platform(
        IPC_MUTATE_APPLICATION)
    self.ipc_replay_binary = utils.application_name_for_platform(
        IPC_REPLAY_APPLICATION)
    self.ipc_message_util_binary_path = os.path.join(
        chrome_application_directory, self.ipc_message_util_binary)
    self.ipc_mutate_binary_path = os.path.join(
        chrome_application_directory, self.ipc_mutate_binary)
    self.ipc_replay_binary_path = os.path.join(
        chrome_application_directory, self.ipc_replay_binary)

  def set_corpus(self):
    input_directory = self.args.input_dir
    entries = os.listdir(input_directory)
    entries = [i for i in entries if i.endswith(utils.IPCDUMP_EXTENSION)]
    self.corpus = [os.path.join(input_directory, entry) for entry in entries]

  def create_mutated_ipcdump_testcase(self):
    ipcdumps = ','.join(random.sample(self.corpus, IPCDUMP_MERGE_LIMIT))
    tmp_ipcdump_testcase = utils.create_temp_file()
    mutated_ipcdump_testcase = (
        utils.random_ipcdump_testcase_path(self.args.output_dir))

    # Concatenate ipcdumps -> tmp_ipcdump.
    cmd = [
      self.ipc_message_util_binary_path,
      ipcdumps,
      tmp_ipcdump_testcase,
    ]
    if subprocess.call(cmd):
      sys.exit('%s failed.' % self.ipc_message_util_binary)

    # Mutate tmp_ipcdump -> mutated_ipcdump.
    cmd = [
      self.ipc_mutate_binary_path,
      tmp_ipcdump_testcase,
      mutated_ipcdump_testcase,
    ]
    if subprocess.call(cmd):
      sys.exit('%s failed.' % self.ipc_mutate_binary)

    utils.create_flags_file(
        mutated_ipcdump_testcase, self.ipc_replay_binary_path)
    os.remove(tmp_ipcdump_testcase)

  def main(self):
    self.parse_arguments()
    self.set_application_paths()
    self.set_corpus()
    for _ in xrange(self.args.no_of_files):
      self.create_mutated_ipcdump_testcase()

    return 0

if __name__ == "__main__":
  fuzzer = MutationalFuzzer()
  sys.exit(fuzzer.main())
