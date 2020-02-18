# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests helpers in devserver_wrapper."""

from __future__ import print_function

import os

import mock

from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import dev_server_wrapper
from chromite.lib import partial_mock


# pylint: disable=protected-access
class TestXbuddyHelpers(cros_test_lib.MockTempDirTestCase):
  """Test xbuddy helper functions."""
  def testGenerateXbuddyRequestForUpdate(self):
    """Test we generate correct xbuddy requests."""
    # Use the latest build when 'latest' is given.
    req = 'xbuddy/latest?for_update=true&return_dir=true'
    self.assertEqual(
        dev_server_wrapper.GenerateXbuddyRequest('latest', 'update'), req)

    # Convert the path starting with 'xbuddy://' to 'xbuddy/'
    path = 'xbuddy://remote/stumpy/version'
    req = 'xbuddy/remote/stumpy/version?for_update=true&return_dir=true'
    self.assertEqual(
        dev_server_wrapper.GenerateXbuddyRequest(path, 'update'), req)

  def testGenerateXbuddyRequestForImage(self):
    """Tests that we generate correct requests to get images."""
    image_path = 'foo/bar/taco'
    self.assertEqual(dev_server_wrapper.GenerateXbuddyRequest(image_path,
                                                              'image'),
                     'xbuddy/foo/bar/taco?return_dir=true')

    image_path = 'xbuddy://foo/bar/taco'
    self.assertEqual(dev_server_wrapper.GenerateXbuddyRequest(image_path,
                                                              'image'),
                     'xbuddy/foo/bar/taco?return_dir=true')

  def testGenerateXbuddyRequestForTranslate(self):
    """Tests that we generate correct requests for translation."""
    image_path = 'foo/bar/taco'
    self.assertEqual(dev_server_wrapper.GenerateXbuddyRequest(image_path,
                                                              'translate'),
                     'xbuddy_translate/foo/bar/taco')

    image_path = 'xbuddy://foo/bar/taco'
    self.assertEqual(dev_server_wrapper.GenerateXbuddyRequest(image_path,
                                                              'translate'),
                     'xbuddy_translate/foo/bar/taco')

  def testConvertTranslatedPath(self):
    """Tests that we convert a translated path to a usable xbuddy path."""
    path = 'remote/latest-canary'
    translated_path = 'taco-release/R36-5761.0.0/chromiumos_test_image.bin'
    self.assertEqual(dev_server_wrapper.ConvertTranslatedPath(path,
                                                              translated_path),
                     'remote/taco-release/R36-5761.0.0/test')

    path = 'latest'
    translated_path = 'taco/R36-5600.0.0/chromiumos_image.bin'
    self.assertEqual(dev_server_wrapper.ConvertTranslatedPath(path,
                                                              translated_path),
                     'local/taco/R36-5600.0.0/dev')

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=True)
  def testTranslatedPathToLocalPath(self, _mock1):
    """Tests that we convert a translated path to a local path correctly."""
    translated_path = 'peppy-release/R33-5116.87.0/chromiumos_image.bin'
    base_path = os.path.join(self.tempdir, 'peppy-release/R33-5116.87.0')

    local_path = os.path.join(base_path, 'chromiumos_image.bin')
    self.assertEqual(
        dev_server_wrapper.TranslatedPathToLocalPath(translated_path,
                                                     self.tempdir),
        local_path)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testTranslatedPathToLocalPathOutsideChroot(self, _mock1):
    """Tests that we convert a translated path when outside the chroot."""
    translated_path = 'peppy-release/R33-5116.87.0/chromiumos_image.bin'
    chroot_dir = os.path.join(constants.SOURCE_ROOT,
                              constants.DEFAULT_CHROOT_DIR)
    static_dir = os.path.join('devserver', 'static')
    chroot_static_dir = os.path.join('/', static_dir)

    local_path = os.path.join(chroot_dir, static_dir, translated_path)
    self.assertEqual(
        dev_server_wrapper.TranslatedPathToLocalPath(
            translated_path, chroot_static_dir),
        local_path)


class TestGetIPv4Address(cros_test_lib.RunCommandTestCase):
  """Tests the GetIPv4Address function."""

  IP_GLOBAL_OUTPUT = """
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 16436 qdisc noqueue state UNKNOWN
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
2: eth0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc pfifo_fast state \
DOWN qlen 1000
    link/ether cc:cc:cc:cc:cc:cc brd ff:ff:ff:ff:ff:ff
3: eth1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP \
qlen 1000
    link/ether dd:dd:dd:dd:dd:dd brd ff:ff:ff:ff:ff:ff
    inet 111.11.11.111/22 brd 111.11.11.255 scope global eth1
    inet6 cdef:0:cdef:cdef:cdef:cdef:cdef:cdef/64 scope global dynamic
       valid_lft 2592000sec preferred_lft 604800sec
"""

  def testGetIPv4AddressParseResult(self):
    """Verifies we can parse the output and get correct IP address."""
    self.rc.AddCmdResult(partial_mock.In('ip'), output=self.IP_GLOBAL_OUTPUT)
    self.assertEqual(dev_server_wrapper.GetIPv4Address(), '111.11.11.111')

  def testGetIPv4Address(self):
    """Tests that correct shell commmand is called."""
    dev_server_wrapper.GetIPv4Address(global_ip=False, dev='eth0')
    self.rc.assertCommandContains(
        ['ip', 'addr', 'show', 'scope', 'host', 'dev', 'eth0'])

    dev_server_wrapper.GetIPv4Address(global_ip=True)
    self.rc.assertCommandContains(['ip', 'addr', 'show', 'scope', 'global'])
