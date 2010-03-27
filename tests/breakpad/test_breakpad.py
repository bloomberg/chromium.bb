# Copyright 2009  The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file

import shutil
import os
import sys
import platform
try:
  import subprocess
except ImportError:
  print 'You need python 2.4 or later to run this script'
  sys.exit(-1)

BPAD_SUBDIR = 'Google Native Client Crash Reporting'
DUMP_SUBDIR = 'Dumps'

def GetPlatformPath():
  if platform.system() in ('Linux',):
    skip=1
  elif platform.system() in ('Windows', 'Microsoft', 'CYGWIN_NT-5.1'):
    skip=0
    dumpPath = os.path.join(os.environ['TEMP'], BPAD_SUBDIR, DUMP_SUBDIR)
  elif platform.system() in ('Darwin', 'Mac'):
    skip=1
  else:
    print 'Unsupported platform for breakpad test!'
    sys.exit(-1)
  return (skip,dumpPath)

def test(cmdline,dumpPath):
  if os.access(dumpPath, os.F_OK):
    print 'Clearing ' + dumpPath
    fnames = os.listdir(dumpPath)
    for fname in fnames:
      os.remove(os.path.join(dumpPath,fname))

  print 'Calling ', cmdline, '\n'
  print '\n\n'
  subprocess.call(cmdline)

  if os.access(dumpPath, os.F_OK):
    fnames = os.listdir(dumpPath)
    if len(fnames) == 1:
      print 'SUCCESS: Found one dump file'
    elif len(fnames) > 1:
      print 'FAIL: more than one dump file produced'

    print 'Clearing new minidumps from ' + dumpPath
    for fname in fnames:
      os.remove(os.path.join(dumpPath,fname))
  else:
    print 'FAIL: No dump file produced'
    return 1
  return 0

def main(argv):
  command = [argv[1], '-d', argv[2]]
  result = 0

  (skip, dumpPath) = GetPlatformPath()

  if skip == 0:
    result += test(command, dumpPath)
  else:
    print 'SUCCESS, I guess: ' + platform.system() + ' not yet supported'

  return result


if __name__ == '__main__':
  sys.exit(main(sys.argv))
