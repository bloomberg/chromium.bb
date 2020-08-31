# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for cros_install_debug_syms.py"""

from __future__ import print_function

from collections import namedtuple
import os
import sys

from chromite.lib import cros_test_lib
from chromite.scripts import cros_install_debug_syms
from chromite.utils import outcap

pytestmark = cros_test_lib.pytestmark_inside_only


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


SimpleIndex = namedtuple('SimpleIndex', 'header packages')


class InstallDebugSymsTest(cros_test_lib.MockTestCase):
  """Test the parsing of package index"""

  def setUp(self):
    self.local_binhosts = ['/build/something/packages/',
                           'file:///build/somethingelse/packages',
                           'file://localhost/build/another/packages']

    self.remote_binhosts = ['http://domain.com/binhost',
                            'gs://chromeos-stuff/binhost']

  def testGetLocalPackageIndex(self):
    """Check that local binhosts are fetched correctly."""
    self.PatchObject(cros_install_debug_syms.binpkg, 'GrabLocalPackageIndex',
                     return_value=SimpleIndex({}, {}))
    self.PatchObject(cros_install_debug_syms.os.path, 'isdir',
                     return_value=True)
    for binhost in self.local_binhosts:
      cros_install_debug_syms.GetPackageIndex(binhost)

  def testGetRemotePackageIndex(self):
    """Check that remote binhosts are fetched correctly."""
    self.PatchObject(cros_install_debug_syms.binpkg, 'GrabRemotePackageIndex',
                     return_value=SimpleIndex({}, {}))
    for binhost in self.remote_binhosts:
      cros_install_debug_syms.GetPackageIndex(binhost)

  def testListRemoteBinhost(self):
    """Check that urls are generated correctly for remote binhosts."""
    chaps_cpv = 'chromeos-base/chaps-0-r2'
    metrics_cpv = 'chromeos-base/metrics-0-r4'

    index = SimpleIndex({}, [{'CPV': 'chromeos-base/shill-0-r1'},
                             {'CPV': chaps_cpv,
                              'DEBUG_SYMBOLS': 'yes'},
                             {'CPV': metrics_cpv,
                              'DEBUG_SYMBOLS': 'yes',
                              'PATH': 'path/to/binpkg.tbz2'}])
    self.PatchObject(cros_install_debug_syms, 'GetPackageIndex',
                     return_value=index)

    for binhost in self.remote_binhosts:
      expected = {chaps_cpv: os.path.join(binhost, chaps_cpv + '.debug.tbz2'),
                  metrics_cpv: os.path.join(binhost,
                                            'path/to/binpkg.debug.tbz2')}
      self.assertEqual(cros_install_debug_syms.ListBinhost(binhost), expected)

  def testListRemoteBinhostWithURI(self):
    """Check that urls are generated correctly when URI is defined."""
    index = SimpleIndex({'URI': 'gs://chromeos-prebuilts'},
                        [{'CPV': 'chromeos-base/shill-0-r1',
                          'DEBUG_SYMBOLS': 'yes',
                          'PATH': 'amd64-generic/paladin1234/shill-0-r1.tbz2'}])
    self.PatchObject(cros_install_debug_syms, 'GetPackageIndex',
                     return_value=index)

    binhost = 'gs://chromeos-prebuilts/gizmo-paladin/'
    debug_symbols_url = ('gs://chromeos-prebuilts/amd64-generic'
                         '/paladin1234/shill-0-r1.debug.tbz2')
    self.assertEqual(cros_install_debug_syms.ListBinhost(binhost),
                     {'chromeos-base/shill-0-r1': debug_symbols_url})


class InstallArgsTest(cros_test_lib.MockTestCase):
  """Test InstallArgs utility funcs."""

  def testListInstallArgs(self):
    """Check ListInstallArgs behavior."""
    parser = cros_install_debug_syms.GetParser()
    opts = parser.parse_args(['--board', 'betty', 'sys-fs/fuse'])
    self.PatchObject(cros_install_debug_syms, 'GetInstallArgs', return_value=[
        ('a/b-1', 'gs://bucket/b-1.tbz2'),
        ('c/d-1', 'gs://bucket/d-1.tbz2'),
    ])
    with outcap.OutputCapturer() as cap:
      cros_install_debug_syms.ListInstallArgs(opts, '/foo')
    self.assertEqual('a/b-1 gs://bucket/b-1.tbz2\nc/d-1 gs://bucket/d-1.tbz2\n',
                     cap.GetStdout())

  def testGetInstallArgsList(self):
    """Check GetInstallArgsList behavior."""
    stdout = ('sys-apps/which-2.21 gs://bucket/board/which-2.21.debug.tbz2\n'
              'dev-libs/foo-1-r1 gs://bucket/board/foo-1-r1.debug.tbz2\n')
    rc = self.StartPatcher(cros_test_lib.RunCommandMock())
    rc.AddCmdResult(cmd=['foo', '--list'], stdout=stdout)
    self.assertEqual(
        [['sys-apps/which-2.21', 'gs://bucket/board/which-2.21.debug.tbz2'],
         ['dev-libs/foo-1-r1', 'gs://bucket/board/foo-1-r1.debug.tbz2']],
        cros_install_debug_syms.GetInstallArgsList(['foo']))
