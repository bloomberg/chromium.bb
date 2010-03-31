#!/usr/bin/env python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around
   third_party/WebKit/WebKitTools/Scripts/new-run-webkit-tests"""
import os
import subprocess
import sys

def main():
    if sys.platform == 'cygwin':
        return(
            'ERROR: you are running from cygwin python, which is not\n'
            '       supported by "run_webkit_test.py". You should use python\n'
            '       for Windows (2.5+) to launch this script. A python for\n'
            '       Windows can be found at your depot_tools folder:\n'
            '       [DEPOT_TOOLS_PATH]/python ' + '"' + sys.argv[0] + '" ' +
            ' '.join(sys.argv[1:]))

    cmd = [sys.executable]
    src_dir=os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(
                         os.path.dirname(os.path.abspath(sys.argv[0]))))))
    script_dir=os.path.join(src_dir, "third_party", "WebKit", "WebKitTools",
                            "Scripts")
    script = os.path.join(script_dir, 'new-run-webkit-tests')
    cmd.append(script)
    if '--chromium' not in sys.argv:
        cmd.append('--chromium')
    cmd.extend(sys.argv[1:])
    return subprocess.call(cmd)

if __name__ == '__main__':
    sys.exit(main())

