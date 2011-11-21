#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Makes sure files have the right permissions.

Some developers have broken SCM configurations that flip the svn:executable
permission on for no good reason. Unix developers who run ls --color will then
see .cc files in green and get confused.

To ignore a particular file, add it to WHITELIST_FILES.
To ignore a particular extension, add it to WHITELIST_EXTENSIONS.
To ignore whatever regexps your heart desires, add it WHITELIST_REGEX.

Note that all directory separators must be slashes (Unix-style) and not
backslashes. All directories should be relative to the source root and all
file paths should be only lowercase.
"""

import optparse
import os
import pipes
import re
import stat
import sys

#### USER EDITABLE SECTION STARTS HERE ####

# Files with these extensions are allowed to have executable permissions.
WHITELIST_EXTENSIONS = [
    'bash',
    'bat',
    'dylib',
    'pl',
    'py',
    'rb',
    'sh',
    'sed',
]

# Files that end the following paths are whitelisted too.
WHITELIST_FILES = [
    '/build/gyp_chromium',
    '/build/linux/dump_app_syms',
    '/build/linux/pkg-config-wrapper',
    '/build/mac/strip_from_xcode',
    '/build/mac/strip_save_dsym',
    '/chrome/installer/mac/pkg-dmg',
    '/chrome/tools/build/linux/chrome-wrapper',
    '/chrome/tools/build/mac/build_app_dmg',
    '/chrome/tools/build/mac/clean_up_old_versions',
    '/chrome/tools/build/mac/copy_framework_unversioned',
    '/chrome/tools/build/mac/dump_product_syms',
    '/chrome/tools/build/mac/generate_localizer',
    '/chrome/tools/build/mac/make_sign_sh',
    '/chrome/tools/build/mac/tweak_info_plist',
    '/chrome/tools/build/mac/verify_order',
    '/o3d/build/gyp_o3d',
    '/o3d/gypbuild',
    '/o3d/installer/linux/debian.in/rules',
    '/third_party/icu/source/runconfigureicu',
    '/third_party/lcov/bin/gendesc',
    '/third_party/lcov/bin/genhtml',
    '/third_party/lcov/bin/geninfo',
    '/third_party/lcov/bin/genpng',
    '/third_party/lcov/bin/lcov',
    '/third_party/lcov/bin/mcov',
    '/third_party/lcov-1.9/bin/gendesc',
    '/third_party/lcov-1.9/bin/genhtml',
    '/third_party/lcov-1.9/bin/geninfo',
    '/third_party/lcov-1.9/bin/genpng',
    '/third_party/lcov-1.9/bin/lcov',
    '/third_party/libxml/linux/xml2-config',
    '/third_party/lzma_sdk/executable/7za.exe',
    '/third_party/swig/linux/swig',
    '/third_party/tcmalloc/chromium/src/pprof',
    '/tools/git/post-checkout',
    '/tools/git/post-merge',
    '/tools/ld_bfd/ld',
]

# File names that are always whitelisted.  (These are all autoconf spew.)
WHITELIST_FILENAMES = set((
  'config.guess',
  'config.sub',
  'configure',
  'depcomp',
  'install-sh',
  'missing',
  'mkinstalldirs',
  'scons',
  'naclsdk',
))

# File paths that contain these regexps will be whitelisted as well.
WHITELIST_REGEX = [
    re.compile('/third_party/openssl/'),
    re.compile('/third_party/sqlite/'),
    re.compile('/third_party/xdg-utils/'),
    re.compile('/third_party/yasm/source/patched-yasm/config'),
    re.compile('/third_party/ffmpeg/patched-ffmpeg/tools'),
]

#### USER EDITABLE SECTION ENDS HERE ####

WHITELIST_EXTENSIONS_REGEX = re.compile(r'\.(%s)' %
                                        '|'.join(WHITELIST_EXTENSIONS))

WHITELIST_FILES_REGEX = re.compile(r'(%s)$' % '|'.join(WHITELIST_FILES))

# Set to true for more output. This is set by the command line options.
VERBOSE = False

# Using forward slashes as directory separators, ending in a forward slash.
# Set by the command line options.
BASE_DIRECTORY = ''

# The default if BASE_DIRECTORY is not set on the command line.
DEFAULT_BASE_DIRECTORY = '../../..'

# The directories which contain the sources managed by git.
GIT_SOURCE_DIRECTORY = set()

# The SVN repository url.
SVN_REPO_URL = ''

# Whether we are using SVN or GIT.
IS_SVN = True

# Executable permission mask
EXECUTABLE_PERMISSION = stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH


def IsWhiteListed(file_path):
  """Returns True if file_path is in our whitelist of files to ignore."""
  if WHITELIST_EXTENSIONS_REGEX.match(os.path.splitext(file_path)[1]):
    return True
  if WHITELIST_FILES_REGEX.search(file_path):
    return True
  if os.path.basename(file_path) in WHITELIST_FILENAMES:
    return True
  for regex in WHITELIST_REGEX:
    if regex.search(file_path):
      return True
  return False


def CheckFile(file_path):
  """Checks file_path's permissions.

  Args:
    file_path: The file path to check.

  Returns:
    Either a string describing the error if there was one, or None if the file
    checked out OK.
  """
  if VERBOSE:
    print 'Checking file: ' + file_path

  file_path_lower = file_path.lower()
  if IsWhiteListed(file_path_lower):
    return None

  # Not whitelisted, stat the file and check permissions.
  try:
    st_mode = os.stat(file_path).st_mode
  except IOError, e:
    return 'Failed to stat file: %s' % e
  except OSError, e:
    return 'Failed to stat file: %s' % e

  if EXECUTABLE_PERMISSION & st_mode:
    error = 'Contains executable permission'
    if VERBOSE:
      return '%s: %06o' % (error, st_mode)
    return error
  return None


def ShouldCheckDirectory(dir_path):
  """Determine if we should check the content of dir_path."""
  if not IS_SVN:
    return dir_path in GIT_SOURCE_DIRECTORY
  repo_url = GetSvnRepositoryRoot(dir_path)
  if not repo_url:
    return False
  return repo_url == SVN_REPO_URL


def CheckDirectory(dir_path):
  """Check the files in dir_path; recursively check its subdirectories."""
  # Collect a list of all files and directories to check.
  files_to_check = []
  dirs_to_check = []
  success = True
  contents = os.listdir(dir_path)
  for cur in contents:
    full_path = os.path.join(dir_path, cur)
    if os.path.isdir(full_path) and ShouldCheckDirectory(full_path):
      dirs_to_check.append(full_path)
    elif os.path.isfile(full_path):
      files_to_check.append(full_path)

  # First check all files in this directory.
  for cur_file in files_to_check:
    file_status = CheckFile(cur_file)
    if file_status is not None:
      print 'ERROR in %s\n%s' % (cur_file, file_status)
      success = False

  # Next recurse into the subdirectories.
  for cur_dir in dirs_to_check:
    if not CheckDirectory(cur_dir):
      success = False
  return success


def GetGitSourceDirectory(root):
  """Returns a set of the directories to be checked.

  Args:
    root: The repository root where a .git directory must exist.

  Returns:
    A set of directories which contain sources managed by git.
  """
  git_source_directory = set()
  popen_out = os.popen('cd %s && git ls-files --full-name .' %
                       pipes.quote(root))
  for line in popen_out:
    dir_path = os.path.join(root, os.path.dirname(line))
    git_source_directory.add(dir_path)
  git_source_directory.add(root)
  return git_source_directory


def GetSvnRepositoryRoot(dir_path):
  """Returns the repository root for a directory.

  Args:
    dir_path: A directory where a .svn subdirectory should exist.

  Returns:
    The svn repository that contains dir_path or None.
  """
  svn_dir = os.path.join(dir_path, '.svn')
  if not os.path.isdir(svn_dir):
    return None
  popen_out = os.popen('cd %s && svn info' % pipes.quote(dir_path))
  for line in popen_out:
    if line.startswith('Repository Root: '):
      return line[len('Repository Root: '):].rstrip()
  return None


def main(argv):
  usage = """Usage: python %prog [--root <root>] [tocheck]
  tocheck  Specifies the directory, relative to root, to check. This defaults
           to "." so it checks everything.

Examples:
  python checkperms.py
  python checkperms.py --root /path/to/source chrome"""

  option_parser = optparse.OptionParser(usage=usage)
  option_parser.add_option('--root', dest='base_directory',
                           default=DEFAULT_BASE_DIRECTORY,
                           help='Specifies the repository root. This defaults '
                           'to %default relative to the script file, which '
                           'will normally be the repository root.')
  option_parser.add_option('-v', '--verbose', action='store_true',
                           help='Print debug logging')
  options, args = option_parser.parse_args()

  global VERBOSE
  if options.verbose:
    VERBOSE = True

  # Optional base directory of the repository.
  global BASE_DIRECTORY
  if (not options.base_directory or
      options.base_directory == DEFAULT_BASE_DIRECTORY):
    BASE_DIRECTORY = os.path.abspath(
        os.path.join(os.path.abspath(argv[0]), DEFAULT_BASE_DIRECTORY))
  else:
    BASE_DIRECTORY = os.path.abspath(argv[2])

  # Figure out which directory we have to check.
  if not args:
    # No directory to check specified, use the repository root.
    start_dir = BASE_DIRECTORY
  elif len(args) == 1:
    # Directory specified. Start here. It's supposed to be relative to the
    # base directory.
    start_dir = os.path.abspath(os.path.join(BASE_DIRECTORY, args[0]))
  else:
    # More than one argument, we don't handle this.
    option_parser.print_help()
    return 1

  print 'Using base directory:', BASE_DIRECTORY
  print 'Checking directory:', start_dir

  BASE_DIRECTORY = BASE_DIRECTORY.replace('\\', '/')
  start_dir = start_dir.replace('\\', '/')

  success = True
  if os.path.exists(os.path.join(BASE_DIRECTORY, '.svn')):
    global SVN_REPO_URL
    SVN_REPO_URL = GetSvnRepositoryRoot(BASE_DIRECTORY)
    if not SVN_REPO_URL:
      print 'Cannot determine the SVN repo URL'
      success = False
  elif os.path.exists(os.path.join(BASE_DIRECTORY, '.git')):
    global IS_SVN
    IS_SVN = False
    global GIT_SOURCE_DIRECTORY
    GIT_SOURCE_DIRECTORY = GetGitSourceDirectory(BASE_DIRECTORY)
    if not GIT_SOURCE_DIRECTORY:
      print 'Cannot determine the list of GIT directories'
      success = False
  else:
    print 'Cannot determine the SCM used in %s' % BASE_DIRECTORY
    success = False

  if success:
    success = CheckDirectory(start_dir)
  if not success:
    print '\nFAILED\n'
    return 1
  print '\nSUCCESS\n'
  return 0


if '__main__' == __name__:
  sys.exit(main(sys.argv))
