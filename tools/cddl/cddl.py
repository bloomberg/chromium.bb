# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

def main():
    os.execv('./cddl', ['./cddl'] + sys.argv[1:])

main()
