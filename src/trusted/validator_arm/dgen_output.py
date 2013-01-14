#!/usr/bin/python
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

"""
Some common boilerplates and helper functions for source code generation
in files dgen_test_output.py and dgen_decode_output.py.
"""

HEADER_BOILERPLATE ="""/*
 * Copyright 2013 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE
"""

NOT_TCB_BOILERPLATE="""#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif
"""

NEWLINE_STR="""
"""

COMMENTED_NEWLINE_STR="""
//"""

"""Adds comment '// ' string after newlines."""
def commented_string(str, indent=''):
  sep = NEWLINE_STR + indent + '//'
  str = str.replace(NEWLINE_STR, sep)
  # This second line is a hack to fix that sometimes newlines are
  # represented as '\n'.
  # TODO(karl) Find the cause of this hack, and fix it.
  return str.replace('\\n', sep)

def ifdef_name(filename):
  """ Generates the ifdef name to use for the given filename"""
  return filename.replace("/", "_").replace(".", "_").upper() + "_"
