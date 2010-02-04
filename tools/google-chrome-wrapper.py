#!/usr/bin/python
# -*- python -*-

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# Simple wrapper script for chrome to work around selenium bugs
# and hide platform differences.

import logging
import os
import platform
import sys

######################################################################
# General Config
######################################################################

LOGGER_FORMAT = ('%(levelname)s:'
                 '%(module)s:'
                 '%(lineno)d:'
                 '%(threadName)s '
                 '%(message)s')

logging.basicConfig(level=logging.DEBUG,
                    format=LOGGER_FORMAT)

UNAME = platform.uname()

######################################################################
# Linux Specific Functions
######################################################################
CHROME_LINUX_EXE = os.getenv('CHROME_LINUX_EXE',
                             '/opt/google/chrome/chrome')
CHROME_LINUX_EXTRA_ARGS = ['--internal-nacl']
# currently not used
CHROME_LINUX_EXTRA_LD_LIBRARY_PATH = []
CHROME_LINUX_EXTRA_PATH = '/opt/google/chrome/'

def StartChromeLinux(argv):
  if not os.access(CHROME_LINUX_EXE, os.X_OK):
    logging.fatal('cannot find chrome exe %s', CHROME_LINUX_EXE)
    sys.exit(-1)
  argv_new = [CHROME_LINUX_EXE]
  for a in argv[1:]:
    # NOTE: selenium work around to remove harmful quotes
    if a.startswith('--user-data-dir='):
      a = a.replace('"', '')
    argv_new.append(a)
  argv_new += CHROME_LINUX_EXTRA_ARGS
  logging.info('launching linux chrome: %s', repr(argv_new))
  os.execvp(CHROME_LINUX_EXE, argv_new)


######################################################################
# Main
######################################################################
def main(argv):
  logging.info('chrome wrapper started on %s', repr(UNAME))
  if UNAME[0] == 'Linux':
    StartChromeLinux(argv)
  else:
    logging.fatal('unsupport platform')

if __name__ == '__main__':
  sys.exit(main(sys.argv))
