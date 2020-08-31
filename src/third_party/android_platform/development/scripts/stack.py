#!/usr/bin/env python
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

"""stack symbolizes native crash dumps."""

import getopt
import glob
import logging
import os
import sys

import stack_core
import subprocess
import symbol
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                os.pardir, os.pardir, os.pardir, os.pardir,
                                'build', 'android'))

from pylib import constants

sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                os.pardir, os.pardir, os.pardir, os.pardir,
                                'tools', 'python'))
import llvm_symbolizer

DEFAULT_SYMROOT='/tmp/symbols'
# From: https://source.android.com/source/build-numbers.html
_ANDROID_M_MAJOR_VERSION=6

def PrintUsage():
  """Print usage and exit with error."""
  print
  print "  usage: " + sys.argv[0] + " [options] [FILE]"
  print
  print "  --symbols-dir=path"
  print "       the path to a symbols dir, such as"
  print "       =/tmp/out/target/product/dream/symbols"
  print
  print "  --chrome-symbols-dir=path"
  print "       the path to a Chrome symbols dir (can be absolute or relative"
  print "       to src), such as =out/Debug/lib.unstripped"
  print
  print "  --output-directory=path"
  print "       the path to the build output directory, such as out/Debug."
  print "       Ignored if --chrome-symbols-dir is passed."
  print
  print "  --apks-directory=path"
  print "       Overrides the default apks directory. Useful if a bundle APKS"
  print "       file has been unzipped into a temporary directory."
  print
  print "  --symbols-zip=path"
  print "       the path to a symbols zip file, such as"
  print "       =dream-symbols-12345.zip"
  print
  print "  --more-info"
  print "  --less-info"
  print "       Change the level of detail in the output."
  print "       --more-info is slower and more verbose, but more functions will"
  print "       be fully qualified with namespace/classname and have full"
  print "       argument information. Also, the 'stack data' section will be"
  print "       printed."
  print
  print "  --arch=arm|arm64|x64|x86|mips"
  print "       the target architecture"
  print
  print "  --fallback-monochrome"
  print "       fallback to monochrome instead of chrome if fail to detect"
  print "       shared lib which is loaded from APK, this doesn't work for"
  print "       component build."
  print
  print "  --quiet"
  print "       Show less logging"
  print
  print "  --verbose"
  print "       enable extra logging, particularly for debugging failed"
  print "       symbolization"
  print
  print "  FILE should contain a stack trace in it somewhere"
  print "       the tool will find that and re-print it with"
  print "       source files and line numbers.  If you don't"
  print "       pass FILE, or if file is -, it reads from"
  print "       stdin."
  print
  sys.exit(1)

def UnzipSymbols(symbolfile, symdir=None):
  """Unzips a file to DEFAULT_SYMROOT and returns the unzipped location.

  Args:
    symbolfile: The .zip file to unzip
    symdir: Optional temporary directory to use for extraction

  Returns:
    A tuple containing (the directory into which the zip file was unzipped,
    the path to the "symbols" directory in the unzipped file).  To clean
    up, the caller can delete the first element of the tuple.

  Raises:
    SymbolDownloadException: When the unzip fails.
  """
  if not symdir:
    symdir = "%s/%s" % (DEFAULT_SYMROOT, hash(symbolfile))
  if not os.path.exists(symdir):
    os.makedirs(symdir)

  logging.info('extracting %s...', symbolfile)
  saveddir = os.getcwd()
  os.chdir(symdir)
  try:
    unzipcode = subprocess.call(["unzip", "-qq", "-o", symbolfile])
    if unzipcode > 0:
      os.remove(symbolfile)
      raise SymbolDownloadException("failed to extract symbol files (%s)."
                                    % symbolfile)
  finally:
    os.chdir(saveddir)

  android_symbols = glob.glob("%s/out/target/product/*/symbols" % symdir)
  if android_symbols:
    return (symdir, android_symbols[0])
  else:
    # This is a zip of Chrome symbols, so symbol.CHROME_SYMBOLS_DIR needs to be
    # updated to point here.
    symbol.CHROME_SYMBOLS_DIR = symdir
    return (symdir, symdir)


def main(argv, test_symbolizer=None):
  try:
    options, arguments = getopt.getopt(argv, "", [
        "more-info", "less-info", "chrome-symbols-dir=", "output-directory=",
        "apks-directory=", "symbols-dir=", "symbols-zip=", "arch=",
        "fallback-monochrome", "verbose", "quiet", "help",
    ])
  except getopt.GetoptError, _:
    PrintUsage()

  zip_arg = None
  more_info = False
  fallback_monochrome = False
  arch_defined = False
  apks_directory = None
  log_level = logging.INFO
  for option, value in options:
    if option == "--help":
      PrintUsage()
    elif option == "--symbols-dir":
      symbol.SYMBOLS_DIR = os.path.abspath(os.path.expanduser(value))
    elif option == "--symbols-zip":
      zip_arg = os.path.abspath(os.path.expanduser(value))
    elif option == "--arch":
      symbol.ARCH = value
      arch_defined = True
    elif option == "--chrome-symbols-dir":
      symbol.CHROME_SYMBOLS_DIR = os.path.join(constants.DIR_SOURCE_ROOT,
                                               value)
    elif option == "--output-directory":
      constants.SetOutputDirectory(os.path.abspath(value))
    elif option == "--apks-directory":
      apks_directory = os.path.abspath(value)
    elif option == "--more-info":
      more_info = True
    elif option == "--less-info":
      more_info = False
    elif option == "--fallback-monochrome":
      fallback_monochrome = True
    elif option == "--verbose":
      log_level = logging.DEBUG
    elif option == "--quiet":
      log_level = logging.WARNING

  if len(arguments) > 1:
    PrintUsage()

  logging.basicConfig(level=log_level)
  # Do an up-front test that the output directory is known.
  if not symbol.CHROME_SYMBOLS_DIR:
    constants.CheckOutputDirectory()

  logging.info('Reading Android symbols from: %s',
               os.path.normpath(symbol.SYMBOLS_DIR))
  chrome_search_path = symbol.GetLibrarySearchPaths()
  logging.info('Searching for Chrome symbols from within: %s',
               ':'.join((os.path.normpath(d) for d in chrome_search_path)))

  rootdir = None
  if zip_arg:
    rootdir, symbol.SYMBOLS_DIR = UnzipSymbols(zip_arg)

  if not arguments or arguments[0] == "-":
    logging.info('Reading native crash info from stdin')
    with llvm_symbolizer.LLVMSymbolizer() as symbolizer:
      stack_core.StreamingConvertTrace(sys.stdin, {}, more_info,
                                       fallback_monochrome, arch_defined,
                                       symbolizer, apks_directory)
  else:
    logging.info('Searching for native crashes in: %s',
                 os.path.realpath(arguments[0]))
    f = open(arguments[0], "r")

    lines = f.readlines()
    f.close()

    # This used to be required when ELF logical addresses did not align with
    # physical addresses, which happened when relocations were converted to APS2
    # format post-link via relocation_packer tool.
    load_vaddrs = {}

    with llvm_symbolizer.LLVMSymbolizer() as symbolizer:
      logging.info('Searching for Chrome symbols from within: %s',
                   ':'.join((os.path.normpath(d) for d in chrome_search_path)))
      stack_core.ConvertTrace(lines, load_vaddrs, more_info,
                              fallback_monochrome, arch_defined,
                              test_symbolizer or symbolizer, apks_directory)

  if rootdir:
    # be a good citizen and clean up...os.rmdir and os.removedirs() don't work
    cmd = "rm -rf \"%s\"" % rootdir
    logging.info('cleaning up (%s)', cmd)
    os.system(cmd)

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))

# vi: ts=2 sw=2
