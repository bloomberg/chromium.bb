# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome OS Image kernel cmdline dump."""

from __future__ import print_function

from chromite.lib import commandline
from chromite.signing.image_signing import imagefile


def main(argv):
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('input_image', type='path',
                      help='Path to input image file')

  options = parser.parse_args(argv)
  options.Freeze()

  imagefile.DumpConfig(options.input_image)
