# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""controller_util unittests."""

from __future__ import print_function

from chromite.api.controller import controller_util
from chromite.api.gen.chromite.api import build_api_test_pb2
from chromite.api.gen.chromiumos import common_pb2
from chromite.cbuildbot import goma_util
from chromite.lib import cros_test_lib
from chromite.lib import portage_util
from chromite.lib.build_target_util import BuildTarget
from chromite.lib.chroot_lib import Chroot


class ParseChrootTest(cros_test_lib.MockTestCase):
  """ParseChroot tests."""

  def testSuccess(self):
    """Test successful handling case."""
    path = '/chroot/path'
    cache_dir = '/cache/dir'
    chrome_root = '/chrome/root'
    use_flags = [{'flag': 'useflag1'}, {'flag': 'useflag2'}]
    features = [{'feature': 'feature1'}, {'feature': 'feature2'}]
    expected_env = {'USE': 'useflag1 useflag2',
                    'FEATURES': 'feature1 feature2',
                    'CHROME_ORIGIN': 'LOCAL_SOURCE'}

    chroot_message = common_pb2.Chroot(path=path, cache_dir=cache_dir,
                                       chrome_dir=chrome_root,
                                       env={'use_flags': use_flags,
                                            'features': features})

    expected = Chroot(path=path, cache_dir=cache_dir, chrome_root=chrome_root,
                      env=expected_env)
    result = controller_util.ParseChroot(chroot_message)

    self.assertEqual(expected, result)


  def testChrootCallToGoma(self):
    """Test calls to goma."""
    path = '/chroot/path'
    cache_dir = '/cache/dir'
    chrome_root = '/chrome/root'
    use_flags = [{'flag': 'useflag1'}, {'flag': 'useflag2'}]
    features = [{'feature': 'feature1'}, {'feature': 'feature2'}]
    goma_test_dir = '/goma/test/dir'
    goma_test_json_string = 'goma_json'
    chromeos_goma_test_dir = '/chromeos/goma/test/dir'

    # Patch goma constructor to avoid creating misc dirs.
    patch = self.PatchObject(goma_util, 'Goma')

    goma_config = common_pb2.GomaConfig(goma_dir=goma_test_dir,
                                        goma_client_json=goma_test_json_string)
    chroot_message = common_pb2.Chroot(path=path, cache_dir=cache_dir,
                                       chrome_dir=chrome_root,
                                       env={'use_flags': use_flags,
                                            'features': features},
                                       goma=goma_config)

    controller_util.ParseChroot(chroot_message)
    patch.assert_called_with(goma_test_dir, goma_test_json_string,
                             stage_name='BuildAPI', chromeos_goma_dir=None,
                             chroot_dir=path,
                             goma_approach=None)

    goma_config.chromeos_goma_dir = chromeos_goma_test_dir
    chroot_message = common_pb2.Chroot(path=path, cache_dir=cache_dir,
                                       chrome_dir=chrome_root,
                                       env={'use_flags': use_flags,
                                            'features': features},
                                       goma=goma_config)

    controller_util.ParseChroot(chroot_message)
    patch.assert_called_with(goma_test_dir, goma_test_json_string,
                             stage_name='BuildAPI',
                             chromeos_goma_dir=chromeos_goma_test_dir,
                             chroot_dir=path,
                             goma_approach=None)

    goma_config.goma_approach = common_pb2.GomaConfig.RBE_PROD
    chroot_message = common_pb2.Chroot(path=path, cache_dir=cache_dir,
                                       chrome_dir=chrome_root,
                                       env={'use_flags': use_flags,
                                            'features': features},
                                       goma=goma_config)

    controller_util.ParseChroot(chroot_message)
    patch.assert_called_with(goma_test_dir, goma_test_json_string,
                             stage_name='BuildAPI',
                             chromeos_goma_dir=chromeos_goma_test_dir,
                             chroot_dir=path,
                             goma_approach=goma_util.GomaApproach(
                                 '?prod', 'goma.chromium.org', True))

    goma_config.goma_approach = common_pb2.GomaConfig.RBE_STAGING
    chroot_message = common_pb2.Chroot(path=path, cache_dir=cache_dir,
                                       chrome_dir=chrome_root,
                                       env={'use_flags': use_flags,
                                            'features': features},
                                       goma=goma_config)

    controller_util.ParseChroot(chroot_message)
    patch.assert_called_with(goma_test_dir, goma_test_json_string,
                             stage_name='BuildAPI',
                             chromeos_goma_dir=chromeos_goma_test_dir,
                             chroot_dir=path,
                             goma_approach=goma_util.GomaApproach(
                                 '?staging', 'staging-goma.chromium.org', True))

  def testWrongMessage(self):
    """Test invalid message type given."""
    with self.assertRaises(AssertionError):
      controller_util.ParseChroot(common_pb2.BuildTarget())


class ParseBuildTargetTest(cros_test_lib.TestCase):
  """ParseBuildTarget tests."""

  def testSuccess(self):
    """Test successful handling case."""
    name = 'board'
    build_target_message = common_pb2.BuildTarget(name=name)
    expected = BuildTarget(name)
    result = controller_util.ParseBuildTarget(build_target_message)

    self.assertEqual(expected, result)

  def testWrongMessage(self):
    """Test invalid message type given."""
    with self.assertRaises(AssertionError):
      controller_util.ParseBuildTarget(common_pb2.Chroot())


class ParseBuildTargetsTest(cros_test_lib.TestCase):
  """ParseBuildTargets tests."""

  def testSuccess(self):
    """Test successful handling case."""
    names = ['foo', 'bar', 'baz']
    message = build_api_test_pb2.TestRequestMessage()
    for name in names:
      message.build_targets.add().name = name

    result = controller_util.ParseBuildTargets(message.build_targets)

    self.assertCountEqual([BuildTarget(name) for name in names], result)

  def testWrongMessage(self):
    """Wrong message type handling."""
    message = common_pb2.Chroot()
    message.env.use_flags.add().flag = 'foo'
    message.env.use_flags.add().flag = 'bar'

    with self.assertRaises(AssertionError):
      controller_util.ParseBuildTargets(message.env.use_flags)


class CPVToPackageInfoTest(cros_test_lib.TestCase):
  """CPVToPackageInfo tests."""

  def testAllFields(self):
    """Test handling when all fields present."""
    pi = common_pb2.PackageInfo()
    cpv = portage_util.SplitCPV('cat/pkg-2.0.0', strict=False)

    controller_util.CPVToPackageInfo(cpv, pi)
    self.assertEqual('cat', pi.category)
    self.assertEqual('pkg', pi.package_name)
    self.assertEqual('2.0.0', pi.version)

  def testNoVersion(self):
    """Test handling when no version given."""
    pi = common_pb2.PackageInfo()
    cpv = portage_util.SplitCPV('cat/pkg', strict=False)

    controller_util.CPVToPackageInfo(cpv, pi)
    self.assertEqual('cat', pi.category)
    self.assertEqual('pkg', pi.package_name)
    self.assertEqual('', pi.version)

  def testPackageOnly(self):
    """Test handling when only given the package name."""
    pi = common_pb2.PackageInfo()
    cpv = portage_util.SplitCPV('pkg', strict=False)

    controller_util.CPVToPackageInfo(cpv, pi)
    self.assertEqual('', pi.category)
    self.assertEqual('pkg', pi.package_name)
    self.assertEqual('', pi.version)


class PackageInfoToCPVTest(cros_test_lib.TestCase):
  """PackageInfoToCPV tests."""

  def testAllFields(self):
    """Quick sanity check it's working properly."""
    pi = common_pb2.PackageInfo()
    pi.package_name = 'pkg'
    pi.category = 'cat'
    pi.version = '2.0.0'

    cpv = controller_util.PackageInfoToCPV(pi)

    self.assertEqual('pkg', cpv.package)
    self.assertEqual('cat', cpv.category)
    self.assertEqual('2.0.0', cpv.version)

  def testNoPackageInfo(self):
    """Test no package info given."""
    self.assertIsNone(controller_util.PackageInfoToCPV(None))

  def testNoPackageName(self):
    """Test no package name given."""
    pi = common_pb2.PackageInfo()
    pi.category = 'cat'
    pi.version = '2.0.0'

    self.assertIsNone(controller_util.PackageInfoToCPV(pi))


class PackageInfoToStringTest(cros_test_lib.TestCase):
  """PackageInfoToString tests."""

  def testAllFields(self):
    """Test all fields present."""
    pi = common_pb2.PackageInfo()
    pi.package_name = 'pkg'
    pi.category = 'cat'
    pi.version = '2.0.0'

    cpv_str = controller_util.PackageInfoToString(pi)

    self.assertEqual('cat/pkg-2.0.0', cpv_str)

  def testNoVersion(self):
    """Test no version provided."""
    pi = common_pb2.PackageInfo()
    pi.package_name = 'pkg'
    pi.category = 'cat'

    cpv_str = controller_util.PackageInfoToString(pi)

    self.assertEqual('cat/pkg', cpv_str)

  def testPackageOnly(self):
    """Test no version provided."""
    pi = common_pb2.PackageInfo()
    pi.package_name = 'pkg'

    cpv_str = controller_util.PackageInfoToString(pi)

    self.assertEqual('pkg', cpv_str)

  def testNoPackageName(self):
    """Test no package name given."""
    pi = common_pb2.PackageInfo()

    with self.assertRaises(ValueError):
      controller_util.PackageInfoToString(pi)


class CPVToStringTest(cros_test_lib.TestCase):
  """CPVToString tests."""

  def testTranslations(self):
    """Test standard translations used."""
    cases = [
        'cat/pkg-2.0.0-r1',
        'cat/pkg-2.0.0',
        'cat/pkg',
        'pkg',
    ]

    for case in cases:
      cpv = portage_util.SplitCPV(case, strict=False)
      # We should end up with as much info as is available, so we should see
      # the original value in each case.
      self.assertEqual(case, controller_util.CPVToString(cpv))

  def testInvalidCPV(self):
    """Test invalid CPV object."""
    cpv = portage_util.SplitCPV('', strict=False)
    with self.assertRaises(ValueError):
      controller_util.CPVToString(cpv)
