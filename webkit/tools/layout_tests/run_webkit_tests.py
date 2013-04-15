#!/usr/bin/env python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around
   third_party/WebKit/Tools/Scripts/run-webkit-tests"""
import os
import subprocess
import sys

def main():
    cmd = [sys.executable]
    src_dir = os.path.abspath(os.path.join(sys.path[0], '..', '..', '..'))
    script_dir=os.path.join(src_dir, "third_party", "WebKit", "Tools",
                            "Scripts")
    script = os.path.join(script_dir, 'run-webkit-tests')
    cmd.append(script)
    if '--chromium' not in sys.argv:
        cmd.append('--chromium')
    cmd.extend(sys.argv[1:])
    return subprocess.call(cmd)

if __name__ == '__main__':
    sys.exit(main())
