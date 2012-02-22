#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

import shutil
import driver_tools
from driver_env import env
from driver_log import Log

def main(argv):
  if len(argv) > 0:
    # --plugin must come after the command flags, but before the filename.
    argv = [argv[0], '--plugin=LLVMgold'] + argv[1:]
  else:
    print get_help(argv)
    return 1
  env.set('ARGS', *argv)
  return driver_tools.RunWithLog('${AR} ${ARGS}', errexit=False)

def get_help(argv):
  return """
Usage: %s [-]{dmpqrstx}[abcDfilMNoPsSTuvV] [member-name] [count] archive-file file...
 commands:
  d            - delete file(s) from the archive
  m[ab]        - move file(s) in the archive
  p            - print file(s) found in the archive
  q[f]         - quick append file(s) to the archive
  r[ab][f][u]  - replace existing or insert new file(s) into the archive
  s            - act as ranlib
  t            - display contents of archive
  x[o]         - extract file(s) from the archive
 command specific modifiers:
  [a]          - put file(s) after [member-name]
  [b]          - put file(s) before [member-name] (same as [i])
  [D]          - use zero for timestamps and uids/gids
  [N]          - use instance [count] of name
  [f]          - truncate inserted file names
  [P]          - use full path names when matching
  [o]          - preserve original dates
  [u]          - only replace files that are newer than current archive contents
 generic modifiers:
  [c]          - do not warn if the library had to be created
  [s]          - create an archive index (cf. ranlib)
  [S]          - do not build a symbol table
  [T]          - make a thin archive
  [v]          - be verbose
  [V]          - display the version number
  @<file>      - read options from <file>
""" % env.getone('SCRIPT_NAME')
