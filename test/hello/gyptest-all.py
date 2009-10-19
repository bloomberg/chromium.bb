#!/usr/bin/env python

"""
Verifies simplest-possible build of a "Hello, world!" program
using an explicit build target of 'all'.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('hello.gyp')

test.build_all('hello.gyp')

test.run_built_executable('hello', stdout="Hello, world!\n")

test.up_to_date('hello.gyp', test.ALL)

test.pass_test()
