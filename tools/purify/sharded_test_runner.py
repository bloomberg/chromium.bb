#!/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# sharded_test_runner

import optparse
import os
import re
import subprocess
import sys

import common

def RunShardedTests(exe, total_shards, params):
  os.environ['GTEST_TOTAL_SHARDS'] = str(total_shards)
  for shard in range(total_shards):
    os.environ['GTEST_SHARD_INDEX'] = str(shard)
    cmd = [exe]
    cmd.extend(params)
    common.RunSubprocess(cmd)


def main():
  exe = sys.argv[1]
  total_shards = int(sys.argv[2])
  params = sys.argv[3:]
  RunShardedTests(exe, total_shards, params)


if __name__ == "__main__":
  main()
