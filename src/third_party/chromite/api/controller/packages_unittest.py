# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""packages controller unit tests."""

from __future__ import print_function

import mock

from chromite.api.controller import packages as packages_controller
from chromite.api.gen.chromite.api import binhost_pb2
from chromite.api.gen.chromite.api import packages_pb2
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import portage_util
from chromite.service import packages as packages_service


class UprevTest(cros_test_lib.MockTestCase):
  """Uprev tests."""

  _PUBLIC = binhost_pb2.OVERLAYTYPE_PUBLIC
  _PRIVATE = binhost_pb2.OVERLAYTYPE_PRIVATE
  _BOTH = binhost_pb2.OVERLAYTYPE_BOTH
  _NONE = binhost_pb2.OVERLAYTYPE_NONE

  def setUp(self):
    self.uprev_patch = self.PatchObject(packages_service, 'uprev_build_targets')

  def _GetRequest(self, targets=None, overlay_type=None, output_dir=None):
    return packages_pb2.UprevPackagesRequest(
        build_targets=[{'name': name} for name in targets or []],
        overlay_type=overlay_type,
        output_dir=output_dir,
    )

  def _GetResponse(self):
    return packages_pb2.UprevPackagesResponse()

  def testNoOverlayTypeFails(self):
    """No overlay type provided should fail."""
    request = self._GetRequest(targets=['foo'])
    response = self._GetResponse()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      packages_controller.Uprev(request, response)

  def testOverlayTypeNoneFails(self):
    """Overlay type none means nothing here and should fail."""
    request = self._GetRequest(targets=['foo'], overlay_type=self._NONE)
    response = self._GetResponse()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      packages_controller.Uprev(request, response)

  def testSuccess(self):
    """Test overall successful argument handling."""
    targets = ['foo', 'bar']
    output_dir = '/tmp/uprev_output_dir'
    expected_type = constants.BOTH_OVERLAYS
    request = self._GetRequest(targets=targets, overlay_type=self._BOTH,
                               output_dir=output_dir)
    response = self._GetResponse()
    uprev_patch = self.PatchObject(packages_service, 'uprev_build_targets')

    packages_controller.Uprev(request, response)

    # Make sure the type is right, verify build targets after.
    uprev_patch.assert_called_once_with(mock.ANY, expected_type, mock.ANY,
                                        output_dir)
    # First argument (build targets) of the first (only) call.
    call_targets = uprev_patch.call_args[0][0]
    self.assertItemsEqual(targets, [t.name for t in call_targets])


class GetBestVisibleTest(cros_test_lib.MockTestCase):
  """GetBestVisible tests."""

  def _GetRequest(self, atom=None):
    return packages_pb2.GetBestVisibleRequest(
        atom=atom,
    )

  def _GetResponse(self):
    return packages_pb2.GetBestVisibleResponse()

  def _MakeCpv(self, category, package, version):
    unused = {
        'cp': None,
        'cpv': None,
        'cpf': None,
        'pv': None,
        'version_no_rev': None,
        'rev': None,
    }
    return portage_util.CPV(
        category=category,
        package=package,
        version=version,
        **unused
    )

  def testNoAtomFails(self):
    """No atom provided should fail."""
    request = self._GetRequest()
    response = self._GetResponse()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      packages_controller.GetBestVisible(request, response)

  def testSuccess(self):
    """Test overall success, argument handling, result forwarding."""
    cpv = self._MakeCpv('category', 'package', 'version')
    self.PatchObject(packages_service, 'get_best_visible', return_value=cpv)

    request = self._GetRequest(atom='chromeos-chrome')
    response = self._GetResponse()

    packages_controller.GetBestVisible(request, response)

    package_info = response.package_info
    self.assertEqual(package_info.category, cpv.category)
    self.assertEqual(package_info.package_name, cpv.package)
    self.assertEqual(package_info.version, cpv.version)
