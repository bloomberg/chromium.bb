#!/usr/bin/python
#
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate .sh runner for libfuzzer test.

Invoked by GN from fuzzer_test.gni.
"""

import argparse
import os
import sys


parser = argparse.ArgumentParser(description="Generate fuzzer launcher.")
parser.add_argument('--fuzzer', required=True)
parser.add_argument('--launcher', required=True)
parser.add_argument('--dict', required=True)
args = parser.parse_args()

out = open(args.launcher, 'w')
out.write("""#!/bin/bash
set -ue
DIR=$(dirname $0)
$DIR/%(fuzzer)s -dict=$DIR/%(dict)s $*
""" % { "fuzzer": args.fuzzer, "dict": args.dict })
out.close()

os.chmod(args.launcher, os.stat(args.launcher).st_mode | 0o111) # chmod a+x
