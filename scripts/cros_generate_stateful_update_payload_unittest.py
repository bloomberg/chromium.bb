# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_generate_stateful_update_payload."""

from __future__ import print_function

import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.scripts import cros_generate_stateful_update_payload


class GenerateStatefulPayloadTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test correct arguments passed to tar."""
  def testDeltaGenerator(self):
    """Test correct arguments propagated to tar call."""

    self.PatchObject(osutils.TempDir, '__enter__', return_value=self.tempdir)
    self.PatchObject(osutils.MountImageContext, '_Mount', autospec=True)
    self.PatchObject(osutils.MountImageContext, '_Unmount', autospec=True)

    fake_partitions = {
        'STATE': cros_build_lib.PartitionInfo(3, 0, 4, 4, 'fs', 'STATE', ''),
    }
    self.PatchObject(cros_build_lib, 'GetImageDiskPartitionInfo',
                     return_value=fake_partitions)

    cros_generate_stateful_update_payload.main([
        '--image_path', '/dev/null',
        '--output_dir', self.tempdir,
    ])

    self.assertCommandContains([
        'tar',
        '-czf',
        os.path.join(self.tempdir, 'stateful.tgz'),
        '--directory=%s/dir-STATE' % self.tempdir,
        '--hard-dereference',
        '--transform=s,^dev_image,dev_image_new,',
        '--transform=s,^var_overlay,var_new,',
        'dev_image',
        'var_overlay',
    ])
