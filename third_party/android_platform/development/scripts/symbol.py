#!/usr/bin/python
#
# Copyright (C) 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Module for looking up symbolic debugging information.

The information can include symbol names, offsets, and source locations.
"""

import glob
import itertools
import logging
import os
import re
import struct
import subprocess
import sys
import zipfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                os.pardir, os.pardir, os.pardir, os.pardir,
                                'build', 'android'))
from pylib import constants
from pylib.symbols import elf_symbolizer


CHROME_SRC = constants.DIR_SOURCE_ROOT
ANDROID_BUILD_TOP = CHROME_SRC
SYMBOLS_DIR = CHROME_SRC
CHROME_SYMBOLS_DIR = None
ARCH = "arm"
TOOLCHAIN_INFO = None
SECONDARY_ABI_OUTPUT_PATH = None

# See:
# http://bugs.python.org/issue14315
# https://hg.python.org/cpython/rev/6dd5e9556a60#l2.8
def PatchZipFile():
  oldDecodeExtra = zipfile.ZipInfo._decodeExtra
  def decodeExtra(self):
    try:
      oldDecodeExtra(self)
    except struct.error:
      pass
  zipfile.ZipInfo._decodeExtra = decodeExtra
PatchZipFile()

def Uname():
  """'uname' for constructing prebuilt/<...> and out/host/<...> paths."""
  uname = os.uname()[0]
  if uname == "Darwin":
    proc = os.uname()[-1]
    if proc == "i386" or proc == "x86_64":
      return "darwin-x86"
    return "darwin-ppc"
  if uname == "Linux":
    return "linux-x86"
  return uname

def GetAapt():
  """Returns the path to aapt.

  Args:
    None

  Returns:
    the pathname of the 'aapt' executable.
  """
  sdk_home = os.path.join('third_party', 'android_tools', 'sdk')
  sdk_home = os.environ.get('SDK_HOME', sdk_home)
  aapt_exe = glob.glob(os.path.join(sdk_home, 'build-tools', '*', 'aapt'))
  if not aapt_exe:
    return None
  return sorted(aapt_exe, key=os.path.getmtime, reverse=True)[0]

def ApkMatchPackageName(aapt, apk_path, package_name):
  """Returns true the APK's package name matches package_name.

  Args:
    aapt: pathname for the 'aapt' executable.
    apk_path: pathname of the APK file.
    package_name: package name to match.

  Returns:
    True if the package name matches or aapt is None, False otherwise.
  """
  if not aapt:
    # Allow false positives
    return True
  aapt_output = subprocess.check_output(
      [aapt, 'dump', 'badging', apk_path]).split('\n')
  package_name_re = re.compile(r'package: .*name=\'(\S*)\'')
  for line in aapt_output:
    match = package_name_re.match(line)
    if match:
      return package_name == match.group(1)
  return False

def PathListJoin(prefix_list, suffix_list):
   """Returns each prefix in prefix_list joined with each suffix in suffix list.

   Args:
     prefix_list: list of path prefixes.
     suffix_list: list of path suffixes.

   Returns:
     List of paths each of which joins a prefix with a suffix.
   """
   return [
       os.path.join(prefix, suffix)
       for suffix in suffix_list for prefix in prefix_list ]


def _GetChromeOutputDirCandidates():
  """Returns a list of output directories to look in."""
  if os.environ.get('CHROMIUM_OUTPUT_DIR') or os.environ.get('BUILDTYPE'):
    return [constants.GetOutDirectory()]
  return [constants.GetOutDirectory(build_type='Debug'),
          constants.GetOutDirectory(build_type='Release')]


def GetCandidates(dirs, filepart, candidate_fun):
  """Returns a list of candidate filenames, sorted by modification time.

  Args:
    dirs: a list of the directory part of the pathname.
    filepart: the file part of the pathname.
    candidate_fun: a function to apply to each candidate, returns a list.

  Returns:
    A list of candidate files ordered by modification time, newest first.
  """
  candidates = PathListJoin(dirs, [filepart])
  logging.debug('GetCandidates: prefiltered candidates = %s' % candidates)
  candidates = list(
      itertools.chain.from_iterable(map(candidate_fun, candidates)))
  candidates.sort(key=os.path.getmtime, reverse=True)
  return candidates

def GetCandidateApks():
  """Returns a list of APKs which could contain the library.

  Args:
    None

  Returns:
    list of APK filename which could contain the library.
  """
  dirs = PathListJoin(_GetChromeOutputDirCandidates(), ['apks'])
  return GetCandidates(dirs, '*.apk', glob.glob)

def GetCrazyLib(apk_filename):
  """Returns the name of the first crazy library from this APK.

  Args:
    apk_filename: name of an APK file.

  Returns:
    Name of the first library which would be crazy loaded from this APK.
  """
  zip_file = zipfile.ZipFile(apk_filename, 'r')
  for filename in zip_file.namelist():
    match = re.match('lib/[^/]*/crazy.(lib.*[.]so)', filename)
    if match:
      return match.group(1)

def GetApkFromLibrary(device_library_path):
  match = re.match(r'.*/([^/]*)-[0-9]+(\/[^/]*)?\.apk$', device_library_path)
  if not match:
    return None
  return match.group(1)

def GetMatchingApks(package_name):
  """Find any APKs which match the package indicated by the device_apk_name.

  Args:
     device_apk_name: name of the APK on the device.

  Returns:
     A list of APK filenames which could contain the desired library.
  """
  return filter(
      lambda candidate_apk:
          ApkMatchPackageName(GetAapt(), candidate_apk, package_name),
      GetCandidateApks())

def MapDeviceApkToLibrary(device_apk_name):
  """Provide a library name which corresponds with device_apk_name.

  Args:
    device_apk_name: name of the APK on the device.

  Returns:
    Name of the library which corresponds to that APK.
  """
  matching_apks = GetMatchingApks(device_apk_name)
  logging.debug('MapDeviceApkToLibrary: matching_apks=%s' % matching_apks)
  for matching_apk in matching_apks:
    crazy_lib = GetCrazyLib(matching_apk)
    if crazy_lib:
      return crazy_lib

def GetLibrarySearchPaths():
  if SECONDARY_ABI_OUTPUT_PATH:
    return PathListJoin([SECONDARY_ABI_OUTPUT_PATH], ['lib.unstripped', 'lib', '.'])
  if CHROME_SYMBOLS_DIR:
    return [CHROME_SYMBOLS_DIR]
  dirs = _GetChromeOutputDirCandidates()
  # GYP places unstripped libraries under out/$BUILDTYPE/lib
  # GN places them under out/$BUILDTYPE/lib.unstripped
  return PathListJoin(dirs, ['lib.unstripped', 'lib', '.'])

def GetCandidateLibraries(library_name):
  """Returns a list of candidate library filenames.

  Args:
    library_name: basename of the library to match.

  Returns:
    A list of matching library filenames for library_name.
  """
  def extant_library(filename):
    if (os.path.exists(filename)
        and elf_symbolizer.ContainsElfMagic(filename)):
      return [filename]
    return []

  candidates = GetCandidates(
      GetLibrarySearchPaths(), library_name,
      extant_library)
  # For GN, candidates includes both stripped an unstripped libraries. Stripped
  # libraries are always newer. Explicitly look for .unstripped and sort them
  # ahead.
  candidates.sort(key=lambda c: int('unstripped' not in c))
  return candidates


def TranslateLibPath(lib):
  # The filename in the stack trace maybe an APK name rather than a library
  # name. This happens when the library was loaded directly from inside the
  # APK. If this is the case we try to figure out the library name by looking
  # for a matching APK file and finding the name of the library in contains.
  # The name of the APK file on the device is of the form
  # <package_name>-<number>.apk. The APK file on the host may have any name
  # so we look at the APK badging to see if the package name matches.
  apk = GetApkFromLibrary(lib)
  if apk is not None:
    logging.debug('TranslateLibPath: apk=%s' % apk)
    mapping = MapDeviceApkToLibrary(apk)
    if mapping:
      lib = mapping

  # SymbolInformation(lib, addr) receives lib as the path from symbols
  # root to the symbols file. This needs to be translated to point to the
  # correct .so path. If the user doesn't explicitly specify which directory to
  # use, then use the most recently updated one in one of the known directories.
  # If the .so is not found somewhere in CHROME_SYMBOLS_DIR, leave it
  # untranslated in case it is an Android symbol in SYMBOLS_DIR.
  library_name = os.path.basename(lib)

  logging.debug('TranslateLibPath: lib=%s library_name=%s' % (lib, library_name))

  candidate_libraries = GetCandidateLibraries(library_name)
  logging.debug('TranslateLibPath: candidate_libraries=%s' % candidate_libraries)
  if not candidate_libraries:
    return lib

  library_path = os.path.relpath(candidate_libraries[0], SYMBOLS_DIR)
  logging.debug('TranslateLibPath: library_path=%s' % library_path)
  return library_path

def FormatSymbolWithOffset(symbol, offset):
  if offset == 0:
    return symbol
  return "%s+%d" % (symbol, offset)

def SetSecondaryAbiOutputPath(path):
   global SECONDARY_ABI_OUTPUT_PATH
   if SECONDARY_ABI_OUTPUT_PATH and SECONDARY_ABI_OUTPUT_PATH != path:
     raise Exception ("Assign SECONDARY_ABI_OUTPUT_PATH to different value " +
                      " origin: %s new: %s" % ("", path))
   else:
     SECONDARY_ABI_OUTPUT_PATH = path
