# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing classes that wrap artifact downloads."""

from __future__ import print_function

import itertools
import os
import pickle
import shutil
import traceback

import six

from chromite.lib import cros_build_lib
from chromite.lib.xbuddy import artifact_info
from chromite.lib.xbuddy import common_util
from chromite.lib.xbuddy import devserver_constants
from chromite.lib.xbuddy import cherrypy_log_util


# We do a number of things with args/kwargs arguments that confuse pylint.
# pylint: disable=docstring-misnamed-args

AU_NTON_DIR = 'au_nton'

############ Actual filenames of artifacts in Google Storage ############

AU_SUITE_FILE = 'au_control.tar.bz2'
PAYGEN_AU_SUITE_FILE_TEMPLATE = 'paygen_au_%(channel)s_control.tar.bz2'
AUTOTEST_FILE = 'autotest.tar'
CONTROL_FILES_FILE = 'control_files.tar'
AUTOTEST_PACKAGES_FILE = 'autotest_packages.tar'
AUTOTEST_COMPRESSED_FILE = 'autotest.tar.bz2'
AUTOTEST_SERVER_PACKAGE_FILE = 'autotest_server_package.tar.bz2'
DEBUG_SYMBOLS_FILE = 'debug.tgz'
DEBUG_SYMBOLS_ONLY_FILE = 'debug_breakpad.tar.xz'
FACTORY_FILE = 'ChromeOS-factory*.zip'
FACTORY_SHIM_FILE = 'factory_image.zip'
FIRMWARE_FILE = 'firmware_from_source.tar.bz2'
IMAGE_FILE = 'image.zip'
TEST_SUITES_FILE = 'test_suites.tar.bz2'
BASE_IMAGE_FILE = 'chromiumos_base_image.tar.xz'
TEST_IMAGE_FILE = 'chromiumos_test_image.tar.xz'
RECOVERY_IMAGE_FILE = 'recovery_image.tar.xz'
SIGNED_RECOVERY_IMAGE_FILE = 'chromeos_*recovery*mp*.bin'
LIBIOTA_TEST_BINARIES_FILE = 'test_binaries.tar.gz'
LIBIOTA_BOARD_UTILS_FILE = 'board_utils.tar.gz'
QUICK_PROVISION_FILE = 'full_dev_part_*.bin.gz'

############ Actual filenames of Android build artifacts ############

ANDROID_IMAGE_ZIP = r'.*-img-[^-]*\.zip'
ANDROID_RADIO_IMAGE = 'radio.img'
ANDROID_BOOTLOADER_IMAGE = 'bootloader.img'
ANDROID_FASTBOOT = 'fastboot'
ANDROID_TEST_ZIP = r'[^-]*-tests-.*\.zip'
ANDROID_VENDOR_PARTITION_ZIP = r'[^-]*-vendor_partitions-.*\.zip'
ANDROID_AUTOTEST_SERVER_PACKAGE = r'[^-]*-autotest_server_package-.*\.tar.bz2'
ANDROID_TEST_SUITES = r'[^-]*-test_suites-.*\.tar.bz2'
ANDROID_CONTROL_FILES = r'[^-]*-autotest_control_files-.*\.tar'
ANDROID_NATIVETESTS_FILE = r'[^-]*-brillo-tests-.*\.zip'
ANDROID_CONTINUOUS_NATIVE_TESTS_FILE = r'[^-]*-continuous_native_tests-.*\.zip'
ANDROID_CONTINUOUS_INSTRUMENTATION_TESTS_FILE = (
    r'[^-]*-continuous_instrumentation_tests-.*\.zip')
ANDROID_CTS_FILE = 'android-cts.zip'
ANDROID_TARGET_FILES_ZIP = r'[^-]*-target_files-.*\.zip'
ANDROID_DTB_ZIP = r'[^-]*-dtb-.*\.zip'
ANDROID_PUSH_TO_DEVICE_ZIP = 'push_to_device.zip'
ANDROID_SEPOLICY_ZIP = 'sepolicy.zip'

_build_artifact_locks = common_util.LockDict()


class ArtifactDownloadError(Exception):
  """Error used to signify an issue processing an artifact."""


class ArtifactMeta(type):
  """metaclass for an artifact type.

  This metaclass is for class Artifact and its subclasses to have a meaningful
  string composed of class name and the corresponding artifact name, e.g.,
  `Artifact_full_payload`. This helps to better logging, refer to logging in
  method Downloader.Download.
  """

  ARTIFACT_NAME = None

  def __str__(cls):
    return '%s_%s' % (cls.__name__, cls.ARTIFACT_NAME)

  def __repr__(cls):
    return str(cls)


@six.add_metaclass(ArtifactMeta)
class Artifact(cherrypy_log_util.Loggable):
  """Wrapper around an artifact to download using a fetcher.

  The purpose of this class is to download objects from Google Storage
  and install them to a local directory. There are two main functions, one to
  download/prepare the artifacts in to a temporary staging area and the second
  to stage it into its final destination.

  IMPORTANT!  (i) `name' is a glob expression by default (and not a regex), be
  attentive when adding new artifacts; (ii) name matching semantics differ
  between a glob (full name string match) and a regex (partial match).

  Class members:
    fetcher: An object which knows how to fetch the artifact.
    name: Name given for artifact; in fact, it is a pattern that captures the
          names of files contained in the artifact. This can either be an
          ordinary shell-style glob (the default), or a regular expression (if
          is_regex_name is True).
    is_regex_name: Whether the name value is a regex (default: glob).
    build: The version of the build i.e. R26-2342.0.0.
    marker_name: Name used to define the lock marker for the artifacts to
                 prevent it from being re-downloaded. By default based on name
                 but can be overriden by children.
    exception_file_path: Path to a file containing the serialized exception,
                         which was raised in Process method. The file is located
                         in the parent folder of install_dir, since the
                         install_dir will be deleted if the build does not
                         existed.
    install_path: Path to artifact.
    install_subdir: Directory within install_path where the artifact is actually
                    stored.
    install_dir: The final location where the artifact should be staged to.
    single_name: If True the name given should only match one item. Note, if not
                 True, self.name will become a list of items returned.
    installed_files: A list of files that were the final result of downloading
                     and setting up the artifact.
    store_installed_files: Whether the list of installed files is stored in the
                           marker file.
  """

  def __init__(self, name, install_dir, build, install_subdir='',
               is_regex_name=False, optional_name=None, alt_name=None):
    """Constructor.

    Args:
      install_dir: Where to install the artifact.
      name: Identifying name to be used to find/store the artifact.
      build: The name of the build e.g. board/release.
      install_subdir: Directory within install_path where the artifact is
                      actually stored.
      is_regex_name: Whether the name pattern is a regex (default: glob).
      optional_name: An alternative name to find the artifact, which can lead
        to faster download. Unlike |name|, there is no guarantee that an
        artifact named |optional_name| is/will be on Google Storage. If it
        exists, we download it. Otherwise, we fall back to wait for |name|.
      alt_name: Name to use for anonymous callers to download artifacts.
    """
    super(Artifact, self).__init__()

    # In-memory lock to keep the devserver from colliding with itself while
    # attempting to stage the same artifact.
    self._process_lock = None

    self.name = name
    self.is_regex_name = is_regex_name
    self.optional_name = optional_name
    self.alt_name = alt_name
    self.build = build

    self.marker_name = '.' + self._SanitizeName(name)

    exception_file_name = ('.' + self._SanitizeName(build) + self.marker_name +
                           '.exception')
    # The exception file needs to be located in parent folder, since the
    # install_dir will be deleted is the build does not exist.
    self.exception_file_path = os.path.join(os.path.dirname(install_dir),
                                            exception_file_name)

    self.install_path = None

    self.install_dir = install_dir
    self.install_subdir = install_subdir

    self.single_name = True

    self.installed_files = []
    self.store_installed_files = True

  @staticmethod
  def _SanitizeName(name):
    """Sanitizes name to be used for creating a file on the filesystem.

    '.','/' and '*' have special meaning in FS lingo. Replace them with words.

    Args:
      name: A file name/path.

    Returns:
      The sanitized name/path.
    """
    return name.replace('*', 'STAR').replace('.', 'DOT').replace('/', 'SLASH')

  def ArtifactStaged(self):
    """Returns True if artifact is already staged.

    This checks for (1) presence of the artifact marker file, and (2) the
    presence of each installed file listed in this marker. Both must hold for
    the artifact to be considered staged. Note that this method is safe for use
    even if the artifacts were not stageed by this instance, as it is assumed
    that any Artifact instance that did the staging wrote the list of
    files actually installed into the marker.
    """
    marker_file = os.path.join(self.install_dir, self.marker_name)

    # If the marker is missing, it's definitely not staged.
    if not os.path.exists(marker_file):
      self._Log('No marker file, %s is not staged.', self)
      return False

    # We want to ensure that every file listed in the marker is actually there.
    if self.store_installed_files:
      with open(marker_file) as f:
        files = [line.strip() for line in f]

      # Check to see if any of the purportedly installed files are missing, in
      # which case the marker is outdated and should be removed.
      missing_files = [fname for fname in files if not os.path.exists(fname)]
      if missing_files:
        self._Log('***ATTENTION*** %s files listed in %s are missing:\n%s',
                  'All' if len(files) == len(missing_files) else 'Some',
                  marker_file, '\n'.join(missing_files))
        os.remove(marker_file)
        self._Log('Missing files, %s is not staged.', self)
        return False

    self._Log('ArtifactStaged() -> yes, %s is staged.', self)
    return True

  def _MarkArtifactStaged(self):
    """Marks the artifact as staged."""
    with open(os.path.join(self.install_dir, self.marker_name), 'w') as f:
      f.write('\n'.join(self.installed_files))

  def _UpdateName(self, names):
    if self.single_name and len(names) > 1:
      raise ArtifactDownloadError('Too many artifacts match %s' % self.name)

    self.name = names[0]

  def _Setup(self):
    """Process the downloaded content, update the list of installed files."""
    # In this primitive case, what was downloaded (has to be a single file) is
    # what's installed.
    self.installed_files = [self.install_path]

  def _ClearException(self):
    """Delete any existing exception saved for this artifact."""
    if os.path.exists(self.exception_file_path):
      os.remove(self.exception_file_path)

  def _SaveException(self, e):
    """Save the exception and traceback to a file for downloader.IsStaged.

    Args:
      e: Exception object to be saved.
    """
    with open(self.exception_file_path, 'wb') as f:
      pickle.dump('%s\n%s' % (e, str(traceback.format_exc())), f)

  def GetException(self):
    """Retrieve any exception that was raised in Process method.

    Returns:
      An Exception object that was raised when trying to process the artifact.
      Return None if no exception was found.
    """
    if not os.path.exists(self.exception_file_path):
      return None
    with open(self.exception_file_path, 'rb') as f:
      return pickle.load(f)

  def Process(self, downloader, no_wait):
    """Main call point to all artifacts. Downloads and Stages artifact.

    Downloads and Stages artifact from Google Storage to the install directory
    specified in the constructor. It multi-thread safe and does not overwrite
    the artifact if it's already been downloaded or being downloaded. After
    processing, leaves behind a marker to indicate to future invocations that
    the artifact has already been staged based on the name of the artifact.

    Do not override as it modifies important private variables, ensures thread
    safety, and maintains cache semantics.

    Note: this may be a blocking call when the artifact is already in the
    process of being staged.

    Args:
      downloader: A downloader instance containing the logic to download
                  artifacts.
      no_wait: If True, don't block waiting for artifact to exist if we fail to
               immediately find it.

    Raises:
      ArtifactDownloadError: If the artifact fails to download from Google
                             Storage for any reason or that the regexp
                             defined by name is not specific enough.
    """
    if not self._process_lock:
      self._process_lock = _build_artifact_locks.lock(
          os.path.join(self.install_dir, self.name))

    real_install_dir = os.path.join(self.install_dir, self.install_subdir)
    with self._process_lock:
      common_util.MkDirP(real_install_dir)
      if not self.ArtifactStaged():
        # Delete any existing exception saved for this artifact.
        self._ClearException()
        found_artifact = False
        if self.optional_name:
          try:
            # Check if the artifact named |optional_name| exists.
            # Because this artifact may not always exist, don't bother
            # to wait for it (set timeout=1).
            new_names = downloader.Wait(
                self.optional_name, self.is_regex_name, None, timeout=1)
            self._UpdateName(new_names)

          except ArtifactDownloadError:
            self._Log('Unable to download %s; fall back to download %s',
                      self.optional_name, self.name)
          else:
            found_artifact = True

        try:
          # If the artifact should already have been uploaded, don't waste
          # cycles waiting around for it to exist.
          if not found_artifact:
            timeout = 1 if no_wait else 10
            new_names = downloader.Wait(
                self.name, self.is_regex_name, self.alt_name, timeout)
            self._UpdateName(new_names)

          files = self.name if isinstance(self.name, list) else [self.name]
          for filename in files:
            self._Log('Downloading file %s', filename)
            self.install_path = downloader.Fetch(filename, real_install_dir)
          self._Setup()
          self._MarkArtifactStaged()
        except Exception as e:
          # Convert an unknown exception into an ArtifactDownloadError.
          if not isinstance(e, ArtifactDownloadError):
            e = ArtifactDownloadError(e)
          # Save the exception to a file for downloader.IsStaged to retrieve.
          self._SaveException(e)
          raise e
      else:
        self._Log('%s is already staged.', self)

  def __str__(self):
    """String representation for the download."""
    return '%s->%s' % (self.name, self.install_dir)

  def __repr__(self):
    return str(self)


class MultiArtifact(Artifact):
  """Wrapper for artifacts where name matches multiple items.."""

  def __init__(self, *args, **kwargs):
    """Takes Artifact args.

    Args:
      *args: See Artifact documentation.
      **kwargs: See Artifact documentation.
    """
    super(MultiArtifact, self).__init__(*args, **kwargs)
    self.single_name = False

  def _UpdateName(self, names):
    self.name = names if isinstance(names, list) else [names]

  def _Setup(self):
    super(MultiArtifact, self)._Setup()

    self.installed_files = [os.path.join(self.install_dir, self.install_subdir,
                                         name) for name in self.name]


class AUTestPayload(MultiArtifact):
  """Wrapper for AUTest delta payloads which need additional setup."""

  def _Setup(self):
    super(AUTestPayload, self)._Setup()

    # Rename to update.gz.
    # TODO(crbug.com/1008058): Change the devserver such that this renaming is
    # not needed anymore.
    for name in self.name:
      dest_name = (devserver_constants.UPDATE_METADATA_FILE
                   if name.endswith('.json')
                   else devserver_constants.UPDATE_FILE)

      install_path = os.path.join(self.install_dir, self.install_subdir, name)
      new_install_path = os.path.join(self.install_dir, self.install_subdir,
                                      dest_name)
      self._Log('moving %s to %s', install_path, new_install_path)
      shutil.move(install_path, new_install_path)

      # Reflect the rename in the list of installed files.
      self.installed_files.remove(install_path)
      self.installed_files.append(new_install_path)


class BundledArtifact(Artifact):
  """A single build artifact bundle e.g. zip file or tar file."""

  def __init__(self, *args, **kwargs):
    """Takes Artifact args with some additional ones.

    Args:
      *args: See Artifact documentation.
      **kwargs: See Artifact documentation.
      files_to_extract: A list of files to extract. If set to None, extract
                        all files.
      exclude: A list of files to exclude. If None, no files are excluded.
    """
    self._files_to_extract = kwargs.pop('files_to_extract', None)
    self._exclude = kwargs.pop('exclude', None)
    super(BundledArtifact, self).__init__(*args, **kwargs)

    # We modify the marker so that it is unique to what was staged.
    if self._files_to_extract:
      self.marker_name = self._SanitizeName(
          '_'.join(['.' + self.name] + self._files_to_extract))

  def _RunUnzip(self, list_only):
    # Unzip is weird. It expects its args before any excludes and expects its
    # excludes in a list following the -x.
    cmd = ['unzip', '-qql' if list_only else '-o', self.install_path]
    if not list_only:
      cmd += ['-d', self.install_dir]

    if self._files_to_extract:
      cmd.extend(self._files_to_extract)

    if self._exclude:
      cmd.append('-x')
      cmd.extend(self._exclude)

    try:
      return cros_build_lib.run(
          cmd, capture_output=True, encoding='utf-8').stdout.splitlines()
    except cros_build_lib.RunCommandError as e:
      raise ArtifactDownloadError(
          'An error occurred when attempting to unzip %s:\n%s' %
          (self.install_path, e))

  def _Setup(self):
    extract_result = self._Extract()
    if self.store_installed_files:
      # List both the archive and the extracted files.
      self.installed_files.append(self.install_path)
      self.installed_files.extend(extract_result)

  def _Extract(self):
    """Extracts files into the install path."""
    if self.name.endswith('.zip'):
      return self._ExtractZipfile()
    else:
      return self._ExtractTarball()

  def _ExtractZipfile(self):
    """Extracts a zip file using unzip."""
    file_list = [os.path.join(self.install_dir, line[30:].strip())
                 for line in self._RunUnzip(True)
                 if not line.endswith('/')]
    if file_list:
      self._RunUnzip(False)

    return file_list

  def _ExtractTarball(self):
    """Extracts a tarball using tar.

    Detects whether the tarball is compressed or not based on the file
    extension and extracts the tarball into the install_path.
    """
    try:
      return common_util.ExtractTarball(self.install_path, self.install_dir,
                                        files_to_extract=self._files_to_extract,
                                        excluded_files=self._exclude,
                                        return_extracted_files=True)
    except common_util.CommonUtilError as e:
      raise ArtifactDownloadError(str(e))


class AutotestTarball(BundledArtifact):
  """Wrapper around the autotest tarball to download from gsutil."""

  def __init__(self, *args, **kwargs):
    super(AutotestTarball, self).__init__(*args, **kwargs)
    # We don't store/check explicit file lists in Autotest tarball markers;
    # this can get huge and unwieldy, and generally make little sense.
    self.store_installed_files = False

  def _Setup(self):
    """Extracts the tarball into the install path excluding test suites."""
    super(AutotestTarball, self)._Setup()

    # Deal with older autotest packages that may not be bundled.
    autotest_dir = os.path.join(self.install_dir,
                                devserver_constants.AUTOTEST_DIR)
    autotest_pkgs_dir = os.path.join(autotest_dir, 'packages')
    if not os.path.exists(autotest_pkgs_dir):
      os.makedirs(autotest_pkgs_dir)

    if not os.path.exists(os.path.join(autotest_pkgs_dir, 'packages.checksum')):
      cmd = ['autotest/utils/packager.py', '--action=upload', '--repository',
             autotest_pkgs_dir, '--all']
      try:
        cros_build_lib.run(cmd, cwd=self.install_dir)
      except cros_build_lib.RunCommandError as e:
        raise ArtifactDownloadError(
            'Failed to create autotest packages!:\n%s' % e)
    else:
      self._Log('Using pre-generated packages from autotest')


class SignedArtifact(Artifact):
  """Wrapper for signed artifacts which need a path translation."""

  def _Setup(self):
    super(SignedArtifact, self)._Setup()

    # Rename to signed_image.bin.
    install_path = os.path.join(self.install_dir, self.install_subdir,
                                self.name)
    new_install_path = os.path.join(self.install_dir, self.install_subdir,
                                    devserver_constants.SIGNED_IMAGE_FILE)
    shutil.move(install_path, new_install_path)

    # Reflect the rename in the list of installed files.
    self.installed_files.remove(install_path)
    self.installed_files = [new_install_path]


def _CreateNewArtifact(tag, base, name, *fixed_args, **fixed_kwargs):
  """Get a data wrapper that describes an artifact's implementation.

  Args:
    tag: Tag of the artifact, defined in artifact_info.
    base: Class of the artifact, e.g., BundledArtifact.
    name: Name of the artifact, e.g., image.zip.
    *fixed_args: Fixed arguments that are additional to the one used in base
                 class.
    **fixed_kwargs: Fixed keyword arguments that are additional to the one used
                    in base class.

  Returns:
    A data wrapper that describes an artifact's implementation.
  """
  # pylint: disable=super-on-old-class
  class NewArtifact(base):
    """A data wrapper that describes an artifact's implementation."""
    ARTIFACT_TAG = tag
    ARTIFACT_NAME = name

    def __init__(self, *args, **kwargs):
      all_args = fixed_args + args
      all_kwargs = {}
      all_kwargs.update(fixed_kwargs)
      all_kwargs.update(kwargs)
      super(NewArtifact, self).__init__(self.ARTIFACT_NAME,
                                        *all_args, **all_kwargs)

  NewArtifact.__name__ = base.__name__
  return NewArtifact


# TODO(dshi): Refactor the code here to split out the logic of creating the
# artifacts mapping to a different module.
chromeos_artifact_map = {}


def _AddCrOSArtifact(tag, base, name, *fixed_args, **fixed_kwargs):
  """Add a data wrapper for ChromeOS artifacts.

  Add a data wrapper that describes a ChromeOS artifact's implementation to
  chromeos_artifact_map.
  """
  artifact = _CreateNewArtifact(tag, base, name, *fixed_args, **fixed_kwargs)
  chromeos_artifact_map.setdefault(tag, []).append(artifact)


_AddCrOSArtifact(artifact_info.FULL_PAYLOAD, AUTestPayload,
                 r'chromeos_.*_full_dev.*bin(\.json)?\Z', is_regex_name=True,
                 alt_name=[u'chromeos_{build}_{board}_dev.bin',
                           u'chromeos_{build}_{board}_dev.bin.json'])
_AddCrOSArtifact(artifact_info.DELTA_PAYLOAD, AUTestPayload,
                 r'chromeos_.*_delta_dev.*bin(\.json)?\Z', is_regex_name=True,
                 install_subdir=AU_NTON_DIR)
_AddCrOSArtifact(artifact_info.STATEFUL_PAYLOAD, Artifact,
                 devserver_constants.STATEFUL_FILE)
_AddCrOSArtifact(artifact_info.BASE_IMAGE, BundledArtifact, IMAGE_FILE,
                 optional_name=BASE_IMAGE_FILE,
                 files_to_extract=[devserver_constants.BASE_IMAGE_FILE])
_AddCrOSArtifact(artifact_info.RECOVERY_IMAGE, BundledArtifact, IMAGE_FILE,
                 optional_name=RECOVERY_IMAGE_FILE,
                 files_to_extract=[devserver_constants.RECOVERY_IMAGE_FILE])
_AddCrOSArtifact(artifact_info.SIGNED_IMAGE, SignedArtifact,
                 SIGNED_RECOVERY_IMAGE_FILE)
_AddCrOSArtifact(artifact_info.DEV_IMAGE, BundledArtifact, IMAGE_FILE,
                 files_to_extract=[devserver_constants.IMAGE_FILE])
_AddCrOSArtifact(artifact_info.TEST_IMAGE, BundledArtifact, IMAGE_FILE,
                 optional_name=TEST_IMAGE_FILE,
                 files_to_extract=[devserver_constants.TEST_IMAGE_FILE])
_AddCrOSArtifact(artifact_info.AUTOTEST, AutotestTarball, AUTOTEST_FILE,
                 files_to_extract=None, exclude=['autotest/test_suites'])
_AddCrOSArtifact(artifact_info.CONTROL_FILES, BundledArtifact,
                 CONTROL_FILES_FILE)
_AddCrOSArtifact(artifact_info.AUTOTEST_PACKAGES, AutotestTarball,
                 AUTOTEST_PACKAGES_FILE)
_AddCrOSArtifact(artifact_info.TEST_SUITES, BundledArtifact, TEST_SUITES_FILE)
_AddCrOSArtifact(artifact_info.AU_SUITE, BundledArtifact, AU_SUITE_FILE)
_AddCrOSArtifact(artifact_info.AUTOTEST_SERVER_PACKAGE, Artifact,
                 AUTOTEST_SERVER_PACKAGE_FILE)
_AddCrOSArtifact(artifact_info.FIRMWARE, Artifact, FIRMWARE_FILE)
_AddCrOSArtifact(artifact_info.SYMBOLS, BundledArtifact, DEBUG_SYMBOLS_FILE,
                 files_to_extract=['debug/breakpad'])
_AddCrOSArtifact(artifact_info.SYMBOLS_ONLY, BundledArtifact,
                 DEBUG_SYMBOLS_ONLY_FILE,
                 files_to_extract=['debug/breakpad'])
_AddCrOSArtifact(artifact_info.FACTORY_IMAGE, BundledArtifact, FACTORY_FILE,
                 files_to_extract=[devserver_constants.FACTORY_IMAGE_FILE])
_AddCrOSArtifact(artifact_info.FACTORY_SHIM_IMAGE, BundledArtifact,
                 FACTORY_SHIM_FILE,
                 files_to_extract=[devserver_constants.FACTORY_SHIM_IMAGE_FILE])
_AddCrOSArtifact(artifact_info.QUICK_PROVISION, MultiArtifact,
                 QUICK_PROVISION_FILE)

# Add all the paygen_au artifacts in one go.
for c in devserver_constants.CHANNELS:
  _AddCrOSArtifact(artifact_info.PAYGEN_AU_SUITE_TEMPLATE % {'channel': c},
                   BundledArtifact,
                   PAYGEN_AU_SUITE_FILE_TEMPLATE % {'channel': c})

#### Libiota Artifacts ####
_AddCrOSArtifact(artifact_info.LIBIOTA_TEST_BINARIES, Artifact,
                 LIBIOTA_TEST_BINARIES_FILE)
_AddCrOSArtifact(artifact_info.LIBIOTA_BOARD_UTILS, Artifact,
                 LIBIOTA_BOARD_UTILS_FILE)

android_artifact_map = {}


def _AddAndroidArtifact(tag, base, name, *fixed_args, **fixed_kwargs):
  """Add a data wrapper for android artifacts.

  Add a data wrapper that describes an Android artifact's implementation to
  android_artifact_map.
  """
  artifact = _CreateNewArtifact(tag, base, name, *fixed_args, **fixed_kwargs)
  android_artifact_map.setdefault(tag, []).append(artifact)


_AddAndroidArtifact(artifact_info.ANDROID_ZIP_IMAGES, Artifact,
                    ANDROID_IMAGE_ZIP, is_regex_name=True)
_AddAndroidArtifact(artifact_info.ANDROID_RADIO_IMAGE, Artifact,
                    ANDROID_RADIO_IMAGE)
_AddAndroidArtifact(artifact_info.ANDROID_BOOTLOADER_IMAGE, Artifact,
                    ANDROID_BOOTLOADER_IMAGE)
_AddAndroidArtifact(artifact_info.ANDROID_FASTBOOT, Artifact, ANDROID_FASTBOOT)
_AddAndroidArtifact(artifact_info.ANDROID_TEST_ZIP, BundledArtifact,
                    ANDROID_TEST_ZIP, is_regex_name=True)
_AddAndroidArtifact(artifact_info.ANDROID_VENDOR_PARTITION_ZIP, Artifact,
                    ANDROID_VENDOR_PARTITION_ZIP, is_regex_name=True)
_AddAndroidArtifact(artifact_info.AUTOTEST_SERVER_PACKAGE, Artifact,
                    ANDROID_AUTOTEST_SERVER_PACKAGE, is_regex_name=True)
_AddAndroidArtifact(artifact_info.TEST_SUITES, BundledArtifact,
                    ANDROID_TEST_SUITES, is_regex_name=True)
_AddAndroidArtifact(artifact_info.CONTROL_FILES, BundledArtifact,
                    ANDROID_CONTROL_FILES, is_regex_name=True)
_AddAndroidArtifact(artifact_info.ANDROID_NATIVETESTS_ZIP, BundledArtifact,
                    ANDROID_NATIVETESTS_FILE, is_regex_name=True)
_AddAndroidArtifact(artifact_info.ANDROID_CONTINUOUS_NATIVE_TESTS_ZIP,
                    BundledArtifact,
                    ANDROID_CONTINUOUS_NATIVE_TESTS_FILE,
                    is_regex_name=True)
_AddAndroidArtifact(artifact_info.ANDROID_CONTINUOUS_INSTRUMENTATION_TESTS_ZIP,
                    BundledArtifact,
                    ANDROID_CONTINUOUS_INSTRUMENTATION_TESTS_FILE,
                    is_regex_name=True)
_AddAndroidArtifact(artifact_info.ANDROID_CTS_ZIP, BundledArtifact,
                    ANDROID_CTS_FILE)
_AddAndroidArtifact(artifact_info.ANDROID_TARGET_FILES_ZIP, Artifact,
                    ANDROID_TARGET_FILES_ZIP, is_regex_name=True)
_AddAndroidArtifact(artifact_info.ANDROID_DTB_ZIP, Artifact,
                    ANDROID_DTB_ZIP, is_regex_name=True)
_AddAndroidArtifact(artifact_info.ANDROID_PUSH_TO_DEVICE_ZIP,
                    Artifact, ANDROID_PUSH_TO_DEVICE_ZIP)
_AddAndroidArtifact(artifact_info.ANDROID_SEPOLICY_ZIP,
                    Artifact, ANDROID_SEPOLICY_ZIP)

class BaseArtifactFactory(object):
  """A factory class that generates build artifacts from artifact names."""

  def __init__(self, artifact_map, download_dir, artifacts, files, build,
               requested_to_optional_map):
    """Initalizes the member variables for the factory.

    Args:
      artifact_map: A map from artifact names to ImplDescription objects.
      download_dir: A directory to which artifacts are downloaded.
      artifacts: List of artifacts to stage. These artifacts must be
                 defined in artifact_info.py and have a mapping in the
                 ARTIFACT_IMPLEMENTATION_MAP.
      files: List of files to stage. These files are just downloaded and staged
             as files into the download_dir.
      build: The name of the build.
      requested_to_optional_map: A map between an artifact X to a list of
              artifacts Y. If X is requested, all items in Y should also get
              triggered for download.
    """
    self.artifact_map = artifact_map
    self.download_dir = download_dir
    self.artifacts = artifacts
    self.files = files
    self.build = build
    self.requested_to_optional_map = requested_to_optional_map

  def _Artifacts(self, names, is_artifact):
    """Returns the Artifacts from |names|.

    If is_artifact is true, then these names define artifacts that must exist in
    the ARTIFACT_IMPLEMENTATION_MAP. Otherwise, treat as filenames to stage as
    basic BuildArtifacts.

    Args:
      names: A sequence of artifact names.
      is_artifact: Whether this is a named (True) or file (False) artifact.

    Returns:
      An iterable of Artifacts.

    Raises:
      KeyError: if artifact doesn't exist in the artifact map.
    """
    if is_artifact:
      classes = itertools.chain(*(self.artifact_map[name] for name in names))
      return list(cls(self.download_dir, self.build) for cls in classes)
    else:
      return list(Artifact(name, self.download_dir, self.build)
                  for name in names)

  def RequiredArtifacts(self):
    """Returns BuildArtifacts for the factory's artifacts.

    Returns:
      An iterable of BuildArtifacts.

    Raises:
      KeyError: if artifact doesn't exist in ARTIFACT_IMPLEMENTATION_MAP.
    """
    artifacts = []
    if self.artifacts:
      artifacts.extend(self._Artifacts(self.artifacts, True))
    if self.files:
      artifacts.extend(self._Artifacts(self.files, False))

    return artifacts

  def OptionalArtifacts(self):
    """Returns BuildArtifacts that should be cached.

    Returns:
      An iterable of BuildArtifacts.

    Raises:
      KeyError: if an optional artifact doesn't exist in
                ARTIFACT_IMPLEMENTATION_MAP yet defined in
                artifact_info.REQUESTED_TO_OPTIONAL_MAP.
    """
    optional_names = set()
    for artifact_name, optional_list in self.requested_to_optional_map.items():
      # We are already downloading it.
      if artifact_name in self.artifacts:
        optional_names = optional_names.union(optional_list)

    return self._Artifacts(optional_names - set(self.artifacts), True)


class ChromeOSArtifactFactory(BaseArtifactFactory):
  """A factory class that generates ChromeOS build artifacts from names."""

  def __init__(self, download_dir, artifacts, files, build):
    """Pass the ChromeOS artifact map to the base class."""
    super(ChromeOSArtifactFactory, self).__init__(
        chromeos_artifact_map, download_dir, artifacts, files, build,
        artifact_info.CROS_REQUESTED_TO_OPTIONAL_MAP)


class AndroidArtifactFactory(BaseArtifactFactory):
  """A factory class that generates Android build artifacts from names."""

  def __init__(self, download_dir, artifacts, files, build):
    """Pass the Android artifact map to the base class."""
    super(AndroidArtifactFactory, self).__init__(
        android_artifact_map, download_dir, artifacts, files, build,
        artifact_info.ANDROID_REQUESTED_TO_OPTIONAL_MAP)
