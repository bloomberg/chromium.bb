#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This tool creates a tarball with all the sources, but without .svn directories.

It can also remove files which are not strictly required for build, so that
the resulting tarball can be reasonably small (last time it was ~110 MB).

Example usage:

export_tarball.py /foo/bar

The above will create file /foo/bar.tar.bz2.
"""

import optparse
import os
import sys
import tarfile

NONESSENTIAL_DIRS = (
    'chrome/common/extensions/docs',
    'chrome/test/data',
    'chrome/tools/test/reference_build',
    'courgette/testdata',
    'data',
    'native_client/src/trusted/service_runtime/testdata',
    'native_client/tests',
    'net/data/cache_tests',
    'src/chrome/test/data',
    'o3d/documentation',
    'o3d/samples',
    'o3d/tests',
    'third_party/angle/samples/gles2_book',
    'third_party/hunspell_dictionaries',
    'third_party/hunspell/tests',
    'third_party/lighttpd',
    'third_party/sqlite/test',
    'third_party/vc_80',
    'third_party/xdg-utils/tests',
    'third_party/yasm/source/patched-yasm/modules/arch/x86/tests',
    'third_party/yasm/source/patched-yasm/modules/dbgfmts/dwarf2/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/bin/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/coff/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/elf/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/macho/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/rdf/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/win32/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/win64/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/xdf/tests',
    'third_party/WebKit/JavaScriptCore/tests',
    'third_party/WebKit/LayoutTests',
    'v8/test',
    'webkit/data/layout_tests',
    'webkit/tools/test/reference_build',
)

def GetSourceDirectory():
  return os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

# Workaround lack of the exclude parameter in add method in python-2.4.
# TODO(phajdan.jr): remove the workaround when it's not needed on the bot.
class MyTarFile(tarfile.TarFile):
  def set_remove_nonessential_files(self, remove):
    self.__remove_nonessential_files = remove

  def add(self, name, arcname=None, recursive=True, exclude=None):
    head, tail = os.path.split(name)
    if tail in ('.svn', '.git'):
      return

    if self.__remove_nonessential_files:
      for nonessential_dir in NONESSENTIAL_DIRS:
        dir_path = os.path.join(GetSourceDirectory(), nonessential_dir)
        if name.startswith(dir_path):
          return

    tarfile.TarFile.add(self, name, arcname=arcname, recursive=recursive)

def main(argv):
  parser = optparse.OptionParser()
  parser.add_option("--remove-nonessential-files",
                    dest="remove_nonessential_files",
                    action="store_true", default=False)

  options, args = parser.parse_args(argv)

  if len(args) != 1:
    print 'You must provide only one argument: output file name'
    print '(without .tar.bz2 extension).'
    return 1

  if not os.path.exists(GetSourceDirectory()):
    print 'Cannot find the src directory.'
    return 1

  output_fullname = args[0] + '.tar.bz2'
  output_basename = os.path.basename(args[0])

  archive = MyTarFile.open(output_fullname, 'w:bz2')
  archive.set_remove_nonessential_files(options.remove_nonessential_files)
  try:
    archive.add(GetSourceDirectory(), arcname=output_basename)
  finally:
    archive.close()

  return 0

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
