#!/usr/bin/env python

"""
Verifies simplest-possible build of a "Hello, world!" program
using an explicit build target of 'hello'.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('hello.gyp')

test.build('hello.gyp', 'hello')

test.run_built_executable('hello', stdout="Hello, world!\n")

test.up_to_date('hello.gyp', 'hello')

test.pass_test()
