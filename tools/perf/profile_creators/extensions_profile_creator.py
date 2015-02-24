# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import platform
import shutil
import socket
import sys
import tempfile
import time
import urllib2
import zipfile

from telemetry.page import profile_creator

import page_sets

from telemetry import benchmark
from telemetry.page import page_test
from telemetry.page import test_expectations
from telemetry.results import results_options
from telemetry.user_story import user_story_runner

class _ExtensionPageTest(page_test.PageTest):
  """This page test verified that extensions were automatically installed."""
  def __init__(self):
    super(_ExtensionPageTest, self).__init__()
    self._user_story_set = page_sets.Typical25PageSet()

    # No matter how many pages in the PageSet, just perform two test iterations.
    for user_story in self._user_story_set[2:]:
      self._user_story_set.RemoveUserStory(user_story)

    # Have the extensions been installed yet?
    self._extensions_installed = False

    # Expected
    self._expected_extension_count = 0

  def ValidateAndMeasurePage(self, _, tab, results):
    # Superclass override.
    # Profile setup works in 2 phases:
    # Phase 1: When the first page is loaded: we wait for a timeout to allow
    #     all extensions to install and to prime safe browsing and other
    #     caches.  Extensions may open tabs as part of the install process.
    # Phase 2: When the second page loads, user_story_runner closes all tabs -
    #     we are left with one open tab, wait for that to finish loading.

    # Sleep for a bit to allow safe browsing and other data to load +
    # extensions to install.
    if not self._extensions_installed:
      sleep_seconds = 15
      logging.info("Sleeping for %d seconds." % sleep_seconds)
      time.sleep(sleep_seconds)
      self._extensions_installed = True
    else:
      # Phase 2: Wait for tab to finish loading.
      for i in xrange(len(tab.browser.tabs)):
        t = tab.browser.tabs[i]
        t.WaitForDocumentReadyStateToBeComplete()

  def DidRunTest(self, browser, results):
    """Superclass override."""
    super(_ExtensionPageTest, self).DidRunTest(browser,
        results)
    # Do some basic sanity checks to make sure the profile is complete.
    installed_extensions = browser.extensions.keys()
    if not len(installed_extensions) == self._expected_extension_count:
      # Diagnosing errors:
      # Too many extensions: Managed environment may be installing additional
      # extensions.
      raise Exception("Unexpected number of extensions installed in browser",
          installed_extensions)


def _ExternalExtensionsPath(profile_path):
  """Returns the OS-dependent path at which to install the extension deployment
   files.

   |profile_path| is the path of the profile that will be used to launch the
   browser.
   """
  if platform.system() == 'Darwin':
    return str(profile_path) + '/External Extensions'

  else:
    raise NotImplementedError('Extension install on %s is not yet supported' %
        platform.system())


def _DownloadExtension(extension_id, output_dir):
  """Download an extension to disk.

  Args:
    extension_id: the extension id.
    output_dir: Directory to download into.

  Returns:
    Extension file downloaded."""
  extension_download_path = os.path.join(output_dir, "%s.crx" % extension_id)

  # Ideally, the Chrome version would be dynamically extracted from the binary.
  # Instead, we use a Chrome version whose release date is expected to be
  # about a hundred years in the future.
  chrome_version = '1000.0.0.0'
  extension_url = (
      "https://clients2.google.com/service/update2/crx?response=redirect"
      "&prodversion=%s&x=id%%3D%s%%26lang%%3Den-US%%26uc"
      % (chrome_version, extension_id))
  response = urllib2.urlopen(extension_url)
  assert(response.getcode() == 200)

  with open(extension_download_path, "w") as f:
    f.write(response.read())

  return extension_download_path


def _GetExtensionInfoFromCRX(crx_path):
  """Parse an extension archive and return information.

  Note:
    The extension name returned by this function may not be valid
  (e.g. in the case of a localized extension name).  It's use is just
  meant to be informational.

  Args:
    crx_path: path to crx archive to look at.

  Returns:
    Tuple consisting of:
    (crx_version, extension_name)"""
  crx_zip = zipfile.ZipFile(crx_path)
  manifest_contents = crx_zip.read('manifest.json')
  decoded_manifest = json.loads(manifest_contents)
  crx_version = decoded_manifest['version']
  extension_name = decoded_manifest['name']

  return (crx_version, extension_name)


class ExtensionsProfileCreator(profile_creator.ProfileCreator):
  """Abstract base class for profile creators that install extensions.

  Extensions are installed using the mechanism described in
  https://developer.chrome.com/extensions/external_extensions.html .

  Subclasses are meant to be run interactively.
  """
  def __init__(self, extensions_to_install=None, theme_to_install=None):
    super(ExtensionsProfileCreator, self).__init__()

    # List of extensions to install.
    self._extensions_to_install = []
    if extensions_to_install:
      self._extensions_to_install.extend(extensions_to_install)
    if theme_to_install:
      self._extensions_to_install.append(theme_to_install)

    # Directory to download extension files into.
    self._extension_download_dir = None

    # List of files to delete after run.
    self._files_to_cleanup = []

  def Run(self, options):
    # Installing extensions requires that the profile directory exist before
    # the browser is launched.
    if not options.browser_options.profile_dir:
      options.browser_options.profile_dir = tempfile.mkdtemp()
    options.browser_options.disable_default_apps = False

    self._PrepareExtensionInstallFiles(options.browser_options.profile_dir)

    expectations = test_expectations.TestExpectations()
    results = results_options.CreateResults(
        benchmark.BenchmarkMetadata(profile_creator.__class__.__name__),
        options)
    extension_page_test = _ExtensionPageTest()
    extension_page_test._expected_extension_count = len(
        self._extensions_to_install)
    user_story_runner.Run(
        extension_page_test, extension_page_test._user_story_set,
        expectations, options, results)

    self._CleanupExtensionInstallFiles()

    # Check that files on this list exist and have content.
    expected_files = [
        os.path.join('Default', 'Network Action Predictor')]
    for filename in expected_files:
      filename = os.path.join(options.output_profile_path, filename)
      if not os.path.getsize(filename) > 0:
        raise Exception("Profile not complete: %s is zero length." % filename)

    if results.failures:
      logging.warning('Some pages failed.')
      logging.warning('Failed pages:\n%s',
                      '\n'.join(map(str, results.pages_that_failed)))
      raise Exception('ExtensionsProfileCreator failed.')

  def _PrepareExtensionInstallFiles(self, profile_path):
    """Download extension archives and create extension install files."""
    extensions_to_install = self._extensions_to_install
    if not extensions_to_install:
      raise ValueError("No extensions or themes to install:",
          extensions_to_install)

    # Create external extensions path if it doesn't exist already.
    external_extensions_dir = _ExternalExtensionsPath(profile_path)
    if not os.path.isdir(external_extensions_dir):
      os.makedirs(external_extensions_dir)

    self._extension_download_dir = tempfile.mkdtemp()

    num_extensions = len(extensions_to_install)
    for i in range(num_extensions):
      extension_id = extensions_to_install[i]
      logging.info("Downloading %s - %d/%d" % (
          extension_id, (i + 1), num_extensions))
      extension_path = _DownloadExtension(extension_id,
          self._extension_download_dir)
      (version, name) = _GetExtensionInfoFromCRX(extension_path)
      extension_info = {'external_crx' : extension_path,
          'external_version' : version,
          '_comment' : name}
      extension_json_path = os.path.join(external_extensions_dir,
          "%s.json" % extension_id)
      with open(extension_json_path, 'w') as f:
        f.write(json.dumps(extension_info))
        self._files_to_cleanup.append(extension_json_path)

  def _CleanupExtensionInstallFiles(self):
    """Cleanup stray files before exiting."""
    logging.info("Cleaning up stray files")
    for filename in self._files_to_cleanup:
      os.remove(filename)

    if self._extension_download_dir:
      # Simple sanity check to lessen the impact of a stray rmtree().
      if len(self._extension_download_dir.split(os.sep)) < 3:
        raise Exception("Path too shallow: %s" % self._extension_download_dir)
      shutil.rmtree(self._extension_download_dir)
      self._extension_download_dir = None
