#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verifies that the histograms XML file is well-formatted."""

import extract_histograms
import os.path


def main():
  # This will raise an exception if the file is not well-formatted.
  xml_file = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                          'histograms.xml')
  histograms = extract_histograms.ExtractHistograms(xml_file)


if __name__ == '__main__':
  main()

