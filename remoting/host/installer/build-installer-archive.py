#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a zip archive for the Chrome Remote Desktop Host installer.

This script builds a zip file that contains all the files needed to build an
installer for Chrome Remote Desktop Host.

This zip archive is then used by the signing bots to:
(1) Sign the binaries
(2) Build the final installer

TODO(garykac) We should consider merging this with build-webapp.py.
"""

import os
import shutil
import subprocess
import sys
import zipfile


def cleanDir(dir):
  """Deletes and recreates the dir to make sure it is clean.

  Args:
    dir: The directory to clean.
  """
  try:
    shutil.rmtree(dir)
  except OSError:
    if os.path.exists(dir):
      raise
    else:
      pass
  os.makedirs(dir, 0775)


def buildDefDictionary(definitions):
  """Builds the definition dictionary from the VARIABLE=value array.

  Args:
    defs: Array of variable definitions: 'VARIABLE=value'.

    Returns:
      Dictionary with the definitions.
  """
  defs = {}
  for d in definitions:
    (key, val) = d.split('=')
    defs[key] = val
  return defs


def createZip(zip_path, directory):
  """Creates a zipfile at zip_path for the given directory.

  Args:
    zip_path: Path to zip file to create.
    directory: Directory with contents to archive.
  """
  zipfile_base = os.path.splitext(os.path.basename(zip_path))[0]
  zip = zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED)
  for (root, dirs, files) in os.walk(directory):
    for f in files:
      full_path = os.path.join(root, f)
      rel_path = os.path.relpath(full_path, directory)
      zip.write(full_path, os.path.join(zipfile_base, rel_path))
  zip.close()


def copyFileIntoArchive(src_file, out_dir, files_root, dst_file, defs):
  """Copies the src_file into the out_dir, preserving the directory structure.

  Args:
    src_file: Full or relative path to source file to copy.
    out_dir: Target directory where files are copied.
    files_root: Path prefix which is stripped of dst_file before appending
                it to the out_dir.
    dst_file: Relative path (and filename) where src_file should be copied.
    defs: Dictionary of variable definitions.
  """
  root_len = len(files_root)
  local_file_path = dst_file[root_len:]
  full_dst_file = os.path.join(out_dir, local_file_path)
  dst_dir = os.path.dirname(full_dst_file)
  if not os.path.exists(dst_dir):
    os.makedirs(dst_dir, 0775)

  (base, ext) = os.path.splitext(src_file)
  if ext == '.app':
    shutil.copytree(src_file, full_dst_file)
  elif ext in ['.sh', '.packproj', '.plist']:
    copyFileWithDefs(src_file, full_dst_file, defs)
  else:
    shutil.copy2(src_file, full_dst_file)


def copyFileWithDefs(src_file, dst_file, defs):
  """Copies from src_file to dst_file, performing variable substitution.

  Any @@variables@@ in the source are replaced with
  file with the out_dir, preserving the directory structure.

  Args:
    src_file: Full or relative path to source file to copy.
    dst_file: Relative path (and filename) where src_file should be copied.
    defs: Dictionary of variable definitions.
  """
  data = open(src_file, 'r').read()
  for key, val in defs.iteritems():
    try:
      data = data.replace('@@' + key + '@@', val)
    except TypeError:
      print repr(key), repr(val)
  open(dst_file, 'w').write(data)
  shutil.copystat(src_file, dst_file)


def copyZipIntoArchive(out_dir, files_root, zip_file):
  """Expands the zip_file into the out_dir, preserving the directory structure.

  Args:
    out_dir: Target directory where unzipped files are copied.
    files_root: Path prefix which is stripped of zip_file before appending
                it to the out_dir.
    zip_file: Relative path (and filename) to the zip file.
  """
  base_zip_name = os.path.basename(zip_file)

  # We don't use the 'zipfile' module here because it doesn't restore all the
  # file permissions correctly. We use the 'unzip' command manually.
  old_dir = os.getcwd();
  os.chdir(os.path.dirname(zip_file))
  subprocess.call(['unzip', '-qq', '-o', base_zip_name])
  os.chdir(old_dir)

  # Unzip into correct dir in out_dir.
  root_len = len(files_root)
  relative_zip_path = zip_file[root_len:]
  out_zip_path = os.path.join(out_dir, relative_zip_path)
  out_zip_dir = os.path.dirname(out_zip_path)

  (src_dir, ignore1) = os.path.splitext(zip_file)
  (base_dir_name, ignore2) = os.path.splitext(base_zip_name)
  shutil.copytree(src_dir, os.path.join(out_zip_dir, base_dir_name))


def buildHostArchive(temp_dir, zip_path, source_files_root, source_files,
                     gen_files, gen_files_dst, defs):
  """Builds a zip archive with the files needed to build the installer.

  Args:
    temp_dir: Temporary dir used to build up the contents for the archive.
    zip_path: Full path to the zip file to create.
    source_files_root: Path prefix to strip off |files| when adding to archive.
    source_files: The array of files to add to archive. The path structure is
                  preserved (except for the |files_root| prefix).
    gen_files: Full path to binaries to add to archive.
    gen_files_dst: Relative path of where to add binary files in archive.
                   This array needs to parallel |binaries_src|.
    defs: Dictionary of variable definitions.
  """
  cleanDir(temp_dir)

  for file in source_files:
    base_file = os.path.basename(file)
    (base, ext) = os.path.splitext(file)
    if ext == '.zip':
      copyZipIntoArchive(temp_dir, source_files_root, file)
    else:
      copyFileIntoArchive(file, temp_dir, source_files_root, file, defs)

  for bs, bd in zip(gen_files, gen_files_dst):
    copyFileIntoArchive(bs, temp_dir, '', bd, {})

  createZip(zip_path, temp_dir)


def error(msg):
  sys.stderr.write('ERROR: %s' % msg)
  sys.exit(1)

def usage():
  """Display basic usage information."""
  print ('Usage: %s\n'
         '  <temp-dir> <zip-path> <files-root-dir>\n'
         '  --source-files <list of source files...>\n'
         '  --generated-files <list of generated target files...>\n'
         '  --generated-files-dst <dst for each generated file...>\n'
         '  --defs <list of VARIABLE=value definitions...>'
         ) % sys.argv[0]


def main():
  if len(sys.argv) < 3:
    usage()
    error('Too few arguments')

  temp_dir = sys.argv[1]
  zip_path = sys.argv[2]
  source_files_root = sys.argv[3]

  arg_mode = ''
  source_files = []
  generated_files = []
  generated_files_dst = []
  definitions = []
  for arg in sys.argv[4:]:
    if arg == '--source-files':
      arg_mode = 'files'
    elif arg == '--generated-files':
      arg_mode = 'gen-src'
    elif arg == '--generated-files-dst':
      arg_mode = 'gen-dst'
    elif arg == '--defs':
      arg_mode = 'defs'

    elif arg_mode == 'files':
      source_files.append(arg)
    elif arg_mode == 'gen-src':
      generated_files.append(arg)
    elif arg_mode == 'gen-dst':
      generated_files_dst.append(arg)
    elif arg_mode == 'defs':
      definitions.append(arg)
    else:
      usage()
      error('Expected --source-files')

  # Make sure at least one file was specified.
  if len(source_files) == 0 and len(generated_files) == 0:
    error('At least one input file must be specified.')

  # Ensure that source_files_root ends with a directory separator.
  if source_files_root[-1:] != os.sep:
    source_files_root += os.sep

  # Verify that the 2 generated_files arrays have the same number of elements.
  if len(generated_files) != len(generated_files_dst):
    error('len(--generated-files) != len(--generated-files-dst)')

  defs = buildDefDictionary(definitions)

  result = buildHostArchive(temp_dir, zip_path, source_files_root,
                            source_files, generated_files, generated_files_dst,
                            defs)

  return 0


if __name__ == '__main__':
  sys.exit(main())
