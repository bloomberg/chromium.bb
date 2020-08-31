#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import model

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import presubmit_util

def main(argv):
  presubmit_util.DoPresubmitMain(argv, 'structured.xml', 'structured.old.xml',
                                 model.PrettifyXML)


if '__main__' == __name__:
  sys.exit(main(sys.argv))
