# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_generate_update_payload."""

from __future__ import print_function

import sys

from chromite.lib import cros_test_lib
from chromite.lib import partial_mock
from chromite.lib.paygen import paygen_payload_lib

from chromite.scripts import cros_generate_update_payload

pytestmark = cros_test_lib.pytestmark_inside_only


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class CrOSGenerateUpdatePayloadTest(cros_test_lib.MockTestCase):
  """Test correct arguments passed to delta_generator."""

  def testGenerateUpdatePayload(self):
    """Test correct arguments propagated to delta_generator call."""

    paygen_mock = self.PatchObject(paygen_payload_lib, 'GenerateUpdatePayload')

    cros_generate_update_payload.main([
        '--tgt-image', 'foo-tgt-image',
        '--src-image', 'foo-src-image',
        '--output', 'foo-output',
        '--check',
        '--private-key', 'foo-private-key',
        '--work-dir', 'foo-work-dir',
    ])

    paygen_mock.assert_called_once_with(
        partial_mock.HasString('foo-tgt-image'),
        partial_mock.HasString('foo-output'),
        src_image=partial_mock.HasString('foo-src-image'),
        work_dir=partial_mock.HasString('foo-work-dir'),
        private_key=partial_mock.HasString('foo-private-key'),
        check=True)
