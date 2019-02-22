# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test Utils library."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib.paygen import utils


class TestUtils(cros_test_lib.TempDirTestCase):
  """Test utils methods."""

  def testCreateTempFileWithContents(self):
    """Verify that we create a temp file with the right message in it."""

    message = 'Test Message With Rocks In'

    # Create the temp file.
    with utils.CreateTempFileWithContents(message) as temp_file:
      temp_name = temp_file.name

      # Verify the name is valid.
      self.assertExists(temp_name)

      # Verify it has the right contents
      with open(temp_name, 'r') as f:
        contents = f.readlines()

      self.assertEqual([message], contents)

    # Verify the temp file goes away when we close it.
    self.assertNotExists(temp_name)

  def testListdirFullpath(self):
    file_a = os.path.join(self.tempdir, 'a')
    file_b = os.path.join(self.tempdir, 'b')

    with file(file_a, 'w+'):
      pass

    with file(file_b, 'w+'):
      pass

    self.assertEqual(sorted(utils.ListdirFullpath(self.tempdir)),
                     [file_a, file_b])
