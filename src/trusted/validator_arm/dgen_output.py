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

HEADER_BOILERPLATE ="""
/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE
"""

NOT_TCB_BOILERPLATE="""
#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

"""

def ifdef_name(filename):
  """ Generates the ifdef name to use for the given filename"""
  return filename.replace("/", "_").replace(".", "_").upper() + "_"
