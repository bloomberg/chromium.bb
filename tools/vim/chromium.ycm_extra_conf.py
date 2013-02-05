# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Autocompletion config for YouCompleteMe in Chromium.
#
# USAGE:
#
#   1. Install YCM [https://github.com/Valloric/YouCompleteMe]
#          (Googlers should check out [go/ycm])
#
#   2. Point to this config file in your .vimrc:
#          let g:ycm_global_ycm_extra_conf =
#              '<chrome_depot>/src/tools/vim/chromium.ycm_extra_conf.py'
#
#   3. Profit
#
#
# Usage notes:
#
#   * You must use ninja & clang to build Chromium.
#
#   * You must have run gyp_chromium and built Chromium recently.
#
#
# Hacking notes:
#
#   * The purpose of this script is to construct an accurate enough command line
#     for YCM to pass to clang so it can build and extract the symbols.
#
#   * Right now, we only pull the -I and -D flags. That seems to be sufficient
#     for everything I've used it for.
#
#   * That whole ninja & clang thing? We could support other configs if someone
#     were willing to write the correct commands and a parser.
#
#   * This has only been tested on gPrecise.


import os
import subprocess

# Flags from YCM's default config.
flags = [
'-Wall',
'-Wextra',
'-Werror',
'-Wno-long-long',
'-Wno-variadic-macros',
'-Wno-unused-parameter',
'-fexceptions',
'-DNDEBUG',
'-DUSE_CLANG_COMPLETER',
'-std=c++11',
'-x',
'c++',
]


def FindChromeSrcFromFilename(filename):
  """Searches for the root of the Chromium checkout.

  Simply checks parent directories until it finds .gclient and src/.

  Args:
    filename: (String) Path to source file being edited.

  Returns:
    (String) Path of 'src/', or None if unable to find.
  """
  curdir = os.path.normpath(os.path.dirname(filename))
  while not (os.path.exists(os.path.join(curdir, '.gclient'))
             and os.path.exists(os.path.join(curdir, 'src'))):
    nextdir = os.path.normpath(os.path.join(curdir, '..'))
    if nextdir == curdir:
      return None
    curdir = nextdir
  return os.path.join(curdir, 'src')


def GetClangCommandFromNinjaForFilename(chrome_root, filename):
  """Returns the command line to build |filename|.

  Figures out where the .o file for this source file will end up. Then asks
  ninja how it would build that object and parses the result.

  Args:
    chrome_root: (String) Path to src/.
    filename: (String) Path to source file being edited.

  Returns:
    (List of Strings) Command line arguments for clang.
  """
  if not chrome_root:
    return []

  # Ninja needs the path to the source file from the output build directory.
  # Cut off the common part and /.
  subdir_filename = filename[len(chrome_root)+1:]
  rel_filename = os.path.join('..', '..', subdir_filename)

  # Ask ninja how it would build our source file.
  p = subprocess.Popen(['ninja', '-v', '-C', chrome_root + '/out/Release', '-t',
                        'commands', rel_filename + '^'],
                       stdout=subprocess.PIPE)
  stdout, stderr = p.communicate()
  if p.returncode:
    return []

  # Ninja might execute several commands to build something. We want the last
  # clang command.
  clang_line = None
  for line in reversed(stdout.split('\n')):
    if line.startswith('clang'):
      clang_line = line
      break
  if not clang_line:
    return []

  # Parse out the -I and -D flags. These seem to be the only ones that are
  # important for YCM's purposes.
  chrome_flags = []
  for flag in clang_line.split(' '):
    if flag.startswith('-I'):
      # Relative paths need to be resolved, because they're relative to the
      # output dir, not the source.
      if flag[2] == '/':
        chrome_flags.append(flag)
      else:
        abs_path = os.path.normpath(os.path.join(
            chrome_root, 'out', 'Release', flag[2:]))
        chrome_flags.append('-I' + abs_path)
    elif flag.startswith('-D'):
      chrome_flags.append(flag)

  # Also include Chromium's src/, because all of Chromium's includes are
  # relative to that.
  chrome_flags.append('-I' + os.path.join(chrome_root))
  return chrome_flags


def FlagsForFile(filename):
  """This is the main entry point for YCM. Its interface is fixed.

  Args:
    filename: (String) Path to source file being edited.

  Returns:
    (Dictionary)
      'flags': (List of Strings) Command line flags.
      'do_cache': (Boolean) True if the result should be cached.
  """
  chrome_root = FindChromeSrcFromFilename(filename)
  chrome_flags = GetClangCommandFromNinjaForFilename(chrome_root,
                                                     filename)
  final_flags = flags + chrome_flags

  return {
    'flags': final_flags,
    'do_cache': True
  }
