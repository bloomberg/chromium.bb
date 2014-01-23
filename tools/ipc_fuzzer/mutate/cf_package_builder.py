#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates IPC fuzzer packages suitable for uploading to ClusterFuzz. Stores
the packages into chrome build directory. See fuzzer_list below for the list of
fuzzers.
"""

import argparse
import distutils.archive_util
import os
import shutil
import sys
import tempfile

class CFPackageBuilder:
  def __init__(self):
    self.fuzzer_list = [
      'ipc_fuzzer_mut',
      'ipc_fuzzer_gen',
    ]

  def parse_args(self):
    desc = 'Builder of IPC fuzzer packages for ClusterFuzz'
    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument('--out-dir', dest='out_dir', default='out',
                        help='output directory under src/ directory')
    parser.add_argument('--build-type', dest='build_type', default='Release',
                        help='Debug vs. Release build')
    self.args = parser.parse_args()

  def get_paths(self):
    script_path = os.path.realpath(__file__)
    self.mutate_dir = os.path.dirname(script_path)
    src_dir = os.path.join(self.mutate_dir, os.pardir, os.pardir, os.pardir)
    src_dir = os.path.abspath(src_dir)
    out_dir = os.path.join(src_dir, self.args.out_dir)
    self.build_dir = os.path.join(out_dir, self.args.build_type)

  def enter_tmp_workdir(self):
    self.old_cwd = os.getcwd()
    self.work_dir = tempfile.mkdtemp()
    os.chdir(self.work_dir)

  def rm_tmp_workdir(self):
    os.chdir(self.old_cwd)
    shutil.rmtree(self.work_dir)

  def build_package(self, fuzzer):
    os.makedirs(fuzzer)
    fuzzer_src_path = os.path.join(self.mutate_dir, fuzzer + '.py')
    fuzzer_dst_path = os.path.join(fuzzer, 'run.py')
    shutil.copyfile(fuzzer_src_path, fuzzer_dst_path)
    distutils.archive_util.make_zipfile(fuzzer, fuzzer)
    package_name = fuzzer + '.zip'
    shutil.copy(package_name, self.build_dir)
    final_package_path = os.path.join(self.build_dir, package_name)
    print 'Built %s' % final_package_path

  def main(self):
    self.parse_args()
    self.get_paths()
    self.enter_tmp_workdir()
    for fuzzer in self.fuzzer_list:
      self.build_package(fuzzer)
    self.rm_tmp_workdir()
    return 0

if __name__ == "__main__":
  builder = CFPackageBuilder()
  sys.exit(builder.main())
