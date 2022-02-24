# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import six

from gpu_tests import gpu_integration_test


class InfoCollectionTestArgs():
  """Struct-like class for passing args to an InfoCollection test."""

  def __init__(self, expected_vendor_id_str=None, expected_device_id_strs=None):
    self.gpu = None
    self.expected_vendor_id_str = expected_vendor_id_str
    self.expected_device_id_strs = expected_device_id_strs


class InfoCollectionTest(gpu_integration_test.GpuIntegrationTest):
  @classmethod
  def Name(cls):
    return 'info_collection'

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(InfoCollectionTest, cls).AddCommandlineArgs(parser)
    parser.add_option(
        '--expected-device-id',
        action='append',
        dest='expected_device_ids',
        default=[],
        help='The expected device id. Can be specified multiple times.')
    parser.add_option('--expected-vendor-id', help='The expected vendor id')

  @classmethod
  def GenerateGpuTests(cls, options):
    yield ('InfoCollection_basic', '_',
           ('_RunBasicTest',
            InfoCollectionTestArgs(
                expected_vendor_id_str=options.expected_vendor_id,
                expected_device_id_strs=options.expected_device_ids)))
    yield ('InfoCollection_direct_composition', '_',
           ('_RunDirectCompositionTest', InfoCollectionTestArgs()))
    yield ('InfoCollection_dx12_vulkan', '_', ('_RunDX12VulkanTest',
                                               InfoCollectionTestArgs()))
    yield ('InfoCollection_asan_info_surfaced', '_', ('_RunAsanInfoTest',
                                                      InfoCollectionTestArgs()))

  @classmethod
  def SetUpProcess(cls):
    super(cls, InfoCollectionTest).SetUpProcess()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()

  def RunActualGpuTest(self, test_path, *args):
    del test_path  # Unused in this particular GPU test.
    # Make sure the GPU process is started
    self.tab.action_runner.Navigate('chrome:gpu')

    # Gather the IDs detected by the GPU process
    system_info = self.browser.GetSystemInfo()
    if not system_info:
      self.fail("Browser doesn't support GetSystemInfo")

    assert len(args) == 2
    test_func = args[0]
    test_args = args[1]
    assert test_args.gpu is None
    test_args.gpu = system_info.gpu
    getattr(self, test_func)(test_args)

  ######################################
  # Helper functions for the tests below

  def _RunBasicTest(self, test_args):
    device = test_args.gpu.devices[0]
    if not device:
      self.fail("System Info doesn't have a gpu")

    detected_vendor_id = device.vendor_id
    detected_device_id = device.device_id

    # Gather the expected IDs passed on the command line
    if (not test_args.expected_vendor_id_str
        or not test_args.expected_device_id_strs):
      self.fail('Missing --expected-[vendor|device]-id command line args')

    expected_vendor_id = int(test_args.expected_vendor_id_str, 16)
    expected_device_ids = [
        int(id_str, 16) for id_str in test_args.expected_device_id_strs
    ]

    # Check expected and detected GPUs match
    if detected_vendor_id != expected_vendor_id:
      self.fail('Vendor ID mismatch, expected %s but got %s.' %
                (expected_vendor_id, detected_vendor_id))

    if detected_device_id not in expected_device_ids:
      self.fail('Device ID mismatch, expected %s but got %s.' %
                (expected_device_ids, detected_device_id))

  def _RunDirectCompositionTest(self, test_args):
    os_name = self.browser.platform.GetOSName()
    if os_name and os_name.lower() == 'win':
      overlay_bot_config = self.GetOverlayBotConfig()
      aux_attributes = test_args.gpu.aux_attributes
      if not aux_attributes:
        self.fail('GPU info does not have aux_attributes.')
      for field, expected in overlay_bot_config.items():
        detected = aux_attributes.get(field, 'NONE')
        if expected != detected:
          self.fail(
              '%s mismatch, expected %s but got %s.' %
              (field, self._ValueToStr(expected), self._ValueToStr(detected)))

  def _RunDX12VulkanTest(self, _):
    os_name = self.browser.platform.GetOSName()
    if os_name and os_name.lower() == 'win':
      self.RestartBrowserIfNecessaryWithArgs(
          ['--no-delay-for-dx12-vulkan-info-collection'])
      # Need to re-request system info for DX12/Vulkan bits.
      system_info = self.browser.GetSystemInfo()
      if not system_info:
        self.fail("Browser doesn't support GetSystemInfo")
      gpu = system_info.gpu
      if gpu is None:
        raise Exception("System Info doesn't have a gpu")
      aux_attributes = gpu.aux_attributes
      if not aux_attributes:
        self.fail('GPU info does not have aux_attributes.')

      dx12_vulkan_bot_config = self.GetDx12VulkanBotConfig()
      for field, expected in dx12_vulkan_bot_config.items():
        detected = aux_attributes.get(field)
        if expected != detected:
          self.fail(
              '%s mismatch, expected %s but got %s.' %
              (field, self._ValueToStr(expected), self._ValueToStr(detected)))

  def _RunAsanInfoTest(self, _):
    gpu_info = self.browser.GetSystemInfo().gpu
    self.assertIn('is_asan', gpu_info.aux_attributes)

  @staticmethod
  def _ValueToStr(value):
    if isinstance(value, six.string_types):
      return value
    if isinstance(value, bool):
      return 'supported' if value else 'unsupported'
    assert False
    return False

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', 'info_collection_expectations.txt')
    ]


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
