# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Artifacts service.

This service houses the high level business logic for all created artifacts.
"""

from __future__ import print_function

import collections
import fnmatch
import glob
import json
import os
import shutil

from chromite.lib import autotest_util
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import toolchain_util
from chromite.lib.paygen import partition_lib
from chromite.lib.paygen import paygen_payload_lib
from chromite.lib.paygen import paygen_stateful_payload_lib


# Archive type constants.
ARCHIVE_CONTROL_FILES = 'control'
ARCHIVE_PACKAGES = 'packages'
ARCHIVE_SERVER_PACKAGES = 'server_packages'
ARCHIVE_TEST_SUITES = 'test_suites'

CPE_WARNINGS_FILE_TEMPLATE = 'cpe-warnings-chromeos-%s.txt'
CPE_RESULT_FILE_TEMPLATE = 'cpe-chromeos-%s.txt'

IMAGE_TARS = {
    constants.BASE_IMAGE_BIN: constants.BASE_IMAGE_TAR,
    constants.VM_IMAGE_BIN: constants.VM_IMAGE_TAR,
    constants.TEST_IMAGE_BIN: constants.TEST_IMAGE_TAR,
    constants.RECOVERY_IMAGE_BIN: constants.RECOVERY_IMAGE_TAR,
}

TAST_BUNDLE_NAME = 'tast_bundles.tar.bz2'
TAST_COMPRESSOR = cros_build_lib.COMP_BZIP2

CpeResult = collections.namedtuple('CpeResult', ['report', 'warnings'])

PinnedGuestImage = collections.namedtuple('PinnedGuestImage',
                                          ['filename', 'uri'])


class Error(Exception):
  """Base module error."""


class ArchiveBaseDirNotFound(Error):
  """Raised when the archive base directory does not exist.

  This error most likely indicates the board was not built.
  """


class CrosGenerateSysrootError(Error):
  """Error when running CrosGenerateSysroot."""


class NoFilesError(Error):
  """When there are no files to archive."""


def BuildFirmwareArchive(chroot, sysroot, output_directory):
  """Build firmware_from_source.tar.bz2 in chroot's sysroot firmware directory.

  Args:
    chroot (chroot_lib.Chroot): The chroot to be used.
    sysroot (sysroot_lib.Sysroot): The sysroot whose artifacts are being
      archived.
    output_directory (str): The path were the completed archives should be put.

  Returns:
    str|None - The archive file path if created, None otherwise.
  """
  firmware_root = os.path.join(chroot.path, sysroot.path.lstrip(os.sep),
                               'firmware')
  source_list = [os.path.relpath(f, firmware_root)
                 for f in glob.iglob(os.path.join(firmware_root, '*'))]
  if not source_list:
    return None

  archive_file = os.path.join(output_directory, constants.FIRMWARE_ARCHIVE_NAME)
  cros_build_lib.CreateTarball(
      archive_file, firmware_root, compression=cros_build_lib.COMP_BZIP2,
      chroot=chroot.path, inputs=source_list)

  return archive_file


def BundleAutotestFiles(chroot, sysroot, output_directory):
  """Create the Autotest Hardware Test archives.

  Args:
    chroot (chroot_lib.Chroot): The chroot containing the sysroot.
    sysroot (sysroot_lib.Sysroot): The sysroot whose artifacts are being
      archived.
    output_directory (str): The path were the completed archives should be put.

  Returns:
    dict - The paths of the files created in |output_directory| by their type.
  """
  assert sysroot.Exists(chroot=chroot)
  assert output_directory

  # archive_basedir is the base directory where the archive commands are run.
  # We want the folder containing the board's autotest folder.
  archive_basedir = chroot.full_path(sysroot.path,
                                     constants.AUTOTEST_BUILD_PATH)
  archive_basedir = os.path.dirname(archive_basedir)

  if not os.path.exists(archive_basedir):
    raise ArchiveBaseDirNotFound(
        'Archive base directory does not exist: %s' % archive_basedir)

  builder = autotest_util.AutotestTarballBuilder(archive_basedir,
                                                 output_directory)
  return {
      ARCHIVE_CONTROL_FILES: builder.BuildAutotestControlFilesTarball(),
      ARCHIVE_PACKAGES: builder.BuildAutotestPackagesTarball(),
      ARCHIVE_SERVER_PACKAGES: builder.BuildAutotestServerPackageTarball(),
      ARCHIVE_TEST_SUITES: builder.BuildAutotestTestSuitesTarball(),
  }


def BundleEBuildLogsTarball(chroot, sysroot, archive_dir):
  """Builds a tarball containing ebuild logs.

  Args:
    chroot (chroot_lib.Chroot): The chroot to be used.
    sysroot (sysroot_lib.Sysroot): Sysroot whose images are being fetched.
    archive_dir: The directory to drop the tarball in.

  Returns:
    The file name of the output tarball, None if no package found.
  """
  tarball_paths = []
  logs_path = chroot.full_path(sysroot.path, 'tmp/portage')

  if not os.path.isdir(logs_path):
    return None

  if not os.path.exists(os.path.join(logs_path, 'logs')):
    return None

  tarball_paths.append('logs')
  tarball_output = os.path.join(archive_dir, 'ebuild_logs.tar.xz')
  try:
    cros_build_lib.CreateTarball(
        tarball_output, cwd=logs_path, chroot=chroot.path, inputs=tarball_paths)
  except cros_build_lib.CreateTarballError:
    logging.warning('Unable to create logs tarball; ignoring until '
                    'https://crbug.com/999933 is sorted out.')
    return None
  return os.path.basename(tarball_output)


def BundleChromeOSConfig(chroot, sysroot, archive_dir):
  """Outputs the ChromeOS Config payload.

  Args:
    chroot (chroot_lib.Chroot): The chroot to be used.
    sysroot (sysroot_lib.Sysroot): Sysroot whose config is being fetched.
    archive_dir: The directory to drop the config in.

  Returns:
    The file name of the output config, None if no config found.
  """
  config_path = chroot.full_path(sysroot.path,
                                 'usr/share/chromeos-config/yaml/config.yaml')

  if not os.path.exists(config_path):
    return None

  config_output = os.path.join(archive_dir, 'config.yaml')
  shutil.copy(config_path, config_output)
  return os.path.basename(config_output)


def BundleSimpleChromeArtifacts(chroot, sysroot, build_target, output_dir):
  """Gather all of the simple chrome artifacts.

  Args:
    chroot (chroot_lib.Chroot): The chroot to be used.
    sysroot (sysroot_lib.Sysroot): The sysroot.
    build_target (build_target_util.BuildTarget): The sysroot's build target.
    output_dir (str): Where all result files should be stored.
  """
  files = []
  files.extend(CreateChromeRoot(chroot, build_target, output_dir))
  files.append(ArchiveChromeEbuildEnv(sysroot, output_dir))

  return files


def BundleVmFiles(chroot, test_results_dir, output_dir):
  """Gather all of the VM files.

  Args:
    chroot (chroot_lib.Chroot): The chroot to be used.
    test_results_dir (str): Test directory relative to chroot.
    output_dir (str): Where all result files should be stored.
  """
  image_dir = chroot.full_path(test_results_dir)
  archives = ArchiveFilesFromImageDir(image_dir, output_dir)
  return archives


# TODO(mmortensen): Refactor ArchiveFilesFromImageDir to be part of a library
# module. I tried moving it to lib/vm.py but this causes a circular dependency.
def ArchiveFilesFromImageDir(images_dir, archive_path):
  """Archives the files into tarballs if they match a prefix from prefix_list.

  Create and return a list of tarballs from the images_dir of files that match
  VM disk and memory prefixes.

  Args:
    images_dir (str): The directory containing the images to archive.
    archive_path (str): The directory where the archives should be created.

  Returns:
    list[str] - The paths to the tarballs.
  """
  images = []
  for prefix in [constants.VM_DISK_PREFIX, constants.VM_MEM_PREFIX]:
    for path, _, filenames in os.walk(images_dir):
      images.extend([
          os.path.join(path, filename)
          for filename in fnmatch.filter(filenames, prefix + '*')
      ])

  tar_files = []
  for image_path in images:
    image_rel_path = os.path.relpath(image_path, images_dir)
    image_parent_dir = os.path.dirname(image_path)
    image_file = os.path.basename(image_path)
    tarball_path = os.path.join(archive_path,
                                '%s.tar' % image_rel_path.replace('/', '_'))
    # Note that tar will chdir to |image_parent_dir|, so that |image_file|
    # is at the top-level of the tar file.
    cros_build_lib.CreateTarball(
        tarball_path,
        image_parent_dir,
        compression=cros_build_lib.COMP_BZIP2,
        inputs=[image_file])
    tar_files.append(tarball_path)

  return tar_files


def ArchiveChromeEbuildEnv(sysroot, output_dir):
  """Generate Chrome ebuild environment.

  Args:
    sysroot (sysroot_lib.Sysroot): The sysroot where the original environment
      archive can be found.
    output_dir (str): Where the result should be stored.

  Returns:
    str: The path to the archive.

  Raises:
    NoFilesException: When the package cannot be found.
  """
  pkg_dir = os.path.join(sysroot.path, 'var', 'db', 'pkg')
  files = glob.glob(os.path.join(pkg_dir, constants.CHROME_CP) + '-*')
  if not files:
    raise NoFilesError('Failed to find package %s' % constants.CHROME_CP)

  if len(files) > 1:
    logging.warning('Expected one package for %s, found %d',
                    constants.CHROME_CP, len(files))

  chrome_dir = sorted(files)[-1]
  env_bzip = os.path.join(chrome_dir, 'environment.bz2')
  result_path = os.path.join(output_dir, constants.CHROME_ENV_TAR)
  with osutils.TempDir() as tempdir:
    # Convert from bzip2 to tar format.
    bzip2 = cros_build_lib.FindCompressor(cros_build_lib.COMP_BZIP2)
    tempdir_tar_path = os.path.join(tempdir, constants.CHROME_ENV_FILE)
    cros_build_lib.RunCommand([bzip2, '-d', env_bzip, '-c'],
                              log_stdout_to_file=tempdir_tar_path)

    cros_build_lib.CreateTarball(result_path, tempdir)

  return result_path


def ArchiveImages(image_dir, output_dir):
  """Create a .tar.xz archive for each image that has been created.

  Args:
    image_dir (str): The directory where the images are located.
    output_dir (str): The location where the archives should be created.

  Returns:
    list[str]: The list of created file names.
  """
  files = os.listdir(image_dir)

  archives = []
  # Filter down to the ones that exist first.
  images = {img: tar for img, tar in IMAGE_TARS.items() if img in files}
  for img, tar in images.items():
    target = os.path.join(output_dir, tar)
    cros_build_lib.CreateTarball(target, image_dir, inputs=(img,),
                                 print_cmd=False)
    archives.append(tar)

  return archives


def BundleImageZip(output_dir, image_dir):
  """Bundle image.zip.

  Args:
    output_dir (str): The location outside the chroot where the files should be
      stored.
    image_dir (str): The directory containing the image.
  """

  filename = 'image.zip'
  zipfile = os.path.join(output_dir, filename)
  cros_build_lib.RunCommand(['zip', zipfile, '-r', '.'],
                            cwd=image_dir,
                            capture_output=True)
  return filename


def CreateChromeRoot(chroot, build_target, output_dir):
  """Create the chrome sysroot.

  Args:
    chroot (chroot_lib.Chroot): The chroot in which the sysroot should be built.
    build_target (build_target_util.BuildTarget): The build target.
    output_dir (str): The location outside the chroot where the files should be
      stored.

  Returns:
    list[str]: The list of created files.

  Raises:
    CrosGenerateSysrootError: When cros_generate_sysroot does not complete
      successfully.
  """
  chroot_args = chroot.GetEnterArgs()

  extra_env = {'USE': 'chrome_internal'}
  base_dir = os.path.join(chroot.path, 'tmp')
  with osutils.TempDir(base_dir=base_dir) as tempdir:
    in_chroot_path = os.path.relpath(tempdir, chroot.path)
    cmd = ['cros_generate_sysroot', '--out-dir', in_chroot_path, '--board',
           build_target.name, '--deps-only', '--package', constants.CHROME_CP]

    try:
      cros_build_lib.RunCommand(cmd, enter_chroot=True, extra_env=extra_env,
                                chroot_args=chroot_args)
    except cros_build_lib.RunCommandError as e:
      raise CrosGenerateSysrootError(
          'Error encountered when running cros_generate_sysroot: %s' % e, e)

    files = []
    for path in osutils.DirectoryIterator(tempdir):
      if os.path.isfile(path):
        rel_path = os.path.relpath(path, tempdir)
        files.append(os.path.join(output_dir, rel_path))
    osutils.CopyDirContents(tempdir, output_dir, allow_nonempty=True)

    return files


def BundleTestUpdatePayloads(image_path, output_dir):
  """Generate the test update payloads.

  Args:
    image_path (str): The full path to an image file.
    output_dir (str): The path where the payloads should be generated.

  Returns:
    list[str] - The list of generated payloads.
  """
  payloads = GenerateTestPayloads(image_path, output_dir, full=True,
                                  stateful=True, delta=True)
  payloads.extend(GenerateQuickProvisionPayloads(image_path, output_dir))

  return payloads


def GenerateTestPayloads(target_image_path, archive_dir, full=False,
                         delta=False, stateful=False):
  """Generates the payloads for hw testing.

  Args:
    target_image_path (str): The path to the image to generate payloads to.
    archive_dir (str): Where to store payloads we generated.
    full (bool): Generate full payloads.
    delta (bool): Generate delta payloads.
    stateful (bool): Generate stateful payload.

  Returns:
    list[str] - The list of payloads that were generated.
  """
  real_target = os.path.realpath(target_image_path)
  # The path to the target should look something like this:
  # .../link/R37-5952.0.2014_06_12_2302-a1/chromiumos_test_image.bin
  board, os_version = real_target.split('/')[-3:-1]
  prefix = 'chromeos'
  suffix = 'dev.bin'
  generated = []

  if full:
    # Names for full payloads look something like this:
    # chromeos_R37-5952.0.2014_06_12_2302-a1_link_full_dev.bin
    name = '_'.join([prefix, os_version, board, 'full', suffix])
    payload_path = os.path.join(archive_dir, name)
    paygen_payload_lib.GenerateUpdatePayload(target_image_path, payload_path)
    generated.append(payload_path)

  if delta:
    # Names for delta payloads look something like this:
    # chromeos_R37-5952.0.2014_06_12_2302-a1_R37-
    # 5952.0.2014_06_12_2302-a1_link_delta_dev.bin
    name = '_'.join([prefix, os_version, os_version, board, 'delta', suffix])
    payload_path = os.path.join(archive_dir, name)
    paygen_payload_lib.GenerateUpdatePayload(
        target_image_path, payload_path, src_image=target_image_path)
    generated.append(payload_path)

  if stateful:
    generated.append(
        paygen_stateful_payload_lib.GenerateStatefulPayload(target_image_path,
                                                            archive_dir))

  return generated


def GenerateQuickProvisionPayloads(target_image_path, archive_dir):
  """Generates payloads needed for quick_provision script.

  Args:
    target_image_path (str): The path to the image to extract the partitions.
    archive_dir (str): Where to store partitions when generated.

  Returns:
    list[str]: The artifacts that were produced.
  """
  payloads = []
  with osutils.TempDir() as temp_dir:
    # These partitions are mainly used by quick_provision.
    partition_lib.ExtractKernel(
        target_image_path, os.path.join(temp_dir, 'full_dev_part_KERN.bin'))
    partition_lib.ExtractRoot(target_image_path,
                              os.path.join(temp_dir, 'full_dev_part_ROOT.bin'),
                              truncate=False)
    for partition in ('KERN', 'ROOT'):
      source = os.path.join(temp_dir, 'full_dev_part_%s.bin' % partition)
      dest = os.path.join(archive_dir, 'full_dev_part_%s.bin.gz' % partition)
      cros_build_lib.CompressFile(source, dest)
      payloads.append(dest)

  return payloads


def BundleAFDOGenerationArtifacts(is_orderfile, chroot,
                                  build_target, output_dir):
  """Generate artifacts for toolchain-related AFDO artifacts.

  Args:
    is_orderfile (boolean): The generation is for orderfile (True) or
    for AFDO (False).
    chroot (chroot_lib.Chroot): The chroot in which the sysroot should be built.
    build_target (build_target_util.BuildTarget): The build target.
    output_dir (str): The location outside the chroot where the files should be
      stored.

  Returns:
    list[str]: The list of tarballs of artifacts.
  """
  chroot_args = chroot.GetEnterArgs()
  with chroot.tempdir() as tempdir:
    if is_orderfile:
      generate_orderfile = toolchain_util.GenerateChromeOrderfile(
          board=build_target.name,
          output_dir=tempdir,
          chroot_path=chroot.path,
          chroot_args=chroot_args)

      generate_orderfile.Perform()
    else:
      generate_afdo = toolchain_util.GenerateBenchmarkAFDOProfile(
          board=build_target.name,
          output_dir=tempdir,
          chroot_path=chroot.path,
          chroot_args=chroot_args)

      generate_afdo.Perform()

    files = []
    for path in osutils.DirectoryIterator(tempdir):
      if os.path.isfile(path):
        rel_path = os.path.relpath(path, tempdir)
        files.append(os.path.join(output_dir, rel_path))
    osutils.CopyDirContents(tempdir, output_dir, allow_nonempty=True)

    return files


def BundleTastFiles(chroot, sysroot, output_dir):
  """Tar up the Tast private test bundles.

  Args:
    chroot (chroot_lib.Chroot): Chroot containing the sysroot.
    sysroot (sysroot_lib.Sysroot): Sysroot whose files are being archived.
    output_dir: Location for storing the result tarball.

  Returns:
    Path of the generated tarball, or None if there is no private test bundles.
  """
  cwd = os.path.join(chroot.path, sysroot.path.lstrip(os.sep), 'build')

  dirs = []
  for d in ('libexec/tast', 'share/tast'):
    if os.path.exists(os.path.join(cwd, d)):
      dirs.append(d)
  if not dirs:
    return None

  tarball = os.path.join(output_dir, TAST_BUNDLE_NAME)
  cros_build_lib.CreateTarball(tarball, cwd, compression=TAST_COMPRESSOR,
                               chroot=chroot.path, inputs=dirs)

  return tarball


def FetchPinnedGuestImages(chroot, sysroot):
  """Fetch the file names and uris of Guest VM and Container images for testing.

  Args:
    chroot (chroot_lib.Chroot): Chroot where the sysroot lives.
    sysroot (sysroot_lib.Sysroot): Sysroot whose images are being fetched.

  Returns:
    list[PinnedGuestImage] - The pinned guest image uris.
  """
  pins_root = os.path.abspath(
      os.path.join(chroot.path, sysroot.path.lstrip(os.sep),
                   constants.GUEST_IMAGES_PINS_PATH))

  pins = []
  for pin_file in sorted(glob.iglob(os.path.join(pins_root, '*.json'))):
    with open(pin_file) as f:
      pin = json.load(f)

      filename = pin.get(constants.PIN_KEY_FILENAME)
      uri = pin.get(constants.PIN_KEY_GSURI)
      if not filename or not uri:
        logging.warning("Skipping invalid pin file: '%s'.", pin_file)
        logging.debug("'%s' data: filename='%s' uri='%s'", pin_file, filename,
                      uri)
        continue

      pins.append(PinnedGuestImage(filename=filename, uri=uri))

  return pins


def GenerateCpeReport(chroot, sysroot, output_dir):
  """Generate CPE export.

  Args:
    chroot (chroot_lib.Chroot): The chroot where the command is being run.
    sysroot (sysroot_lib.Sysroot): The sysroot whose dependencies are being
        reported.
    output_dir (str): The path where the output files should be written.

  Returns:
    CpeResult: The CPE result instance with the full paths to the report and
      warnings files.
  """
  # Build the command and its args.
  cmd = [
      'cros_extract_deps', '--sysroot', sysroot.path, '--format', 'cpe',
      'virtual/target-os'
  ]

  logging.info('Beginning CPE Export.')
  result = cros_build_lib.RunCommand(
      cmd,
      capture_output=True,
      enter_chroot=True,
      chroot_args=chroot.get_enter_args())
  logging.info('CPE Export Complete.')

  # Write out the report and warnings the export produced.
  # We'll assume the basename for the board name to match how these were built
  # out in the old system.
  # TODO(saklein): Can we remove the board name from the report file names?
  build_target = os.path.basename(sysroot.path)
  report_path = os.path.join(output_dir,
                             CPE_RESULT_FILE_TEMPLATE % build_target)
  warnings_path = os.path.join(output_dir,
                               CPE_WARNINGS_FILE_TEMPLATE % build_target)

  osutils.WriteFile(report_path, result.output)
  osutils.WriteFile(warnings_path, result.error)

  return CpeResult(report=report_path, warnings=warnings_path)
