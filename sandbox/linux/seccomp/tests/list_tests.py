# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys


def get_tests(filename):
    for line in open(filename):
        match = re.match(r"TEST\((\w+)\)", line)
        if match is not None:
            yield match.group(1)


def main(args):
    for name in get_tests(args[0]):
        print '  { "%s", %s },' % (name, name)


if __name__ == "__main__":
    main(sys.argv[1:])
