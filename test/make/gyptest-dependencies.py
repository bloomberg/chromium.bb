#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that toolsets are correctly applied
"""

import os
import TestGyp

# Multiple toolsets are currently only supported by the make generator.
test = TestGyp.TestGyp(formats=['make'])

test.run_gyp('dependencies.gyp')

test.build('dependencies.gyp', test.ALL)

deps_file = test.built_file_path(".deps/out/Default/obj.target/main/main.o.d")
test.must_contain(deps_file, "main.h")

# Build a second time to make sure we generate all.deps.
test.build('dependencies.gyp', test.ALL)

all_deps_file = test.built_file_path(".deps/all.deps")
test.must_contain(all_deps_file, "main.h")
test.must_contain(all_deps_file, "cmd_")

test.pass_test()
