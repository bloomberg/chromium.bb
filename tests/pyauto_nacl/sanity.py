#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_nacl  # Must be imported before pyauto
import pyauto


class NaClTest(pyauto.PyUITest):
  """Tests for NaCl."""

  def testSanity(self):
    """A minimal self test."""
    self.NavigateToURL('about:plugins')


if __name__ == '__main__':
  pyauto_nacl.Main()
