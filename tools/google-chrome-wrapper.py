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
# Common Functions
######################################################################
def StartChrome(default_exe, argv, extra_args):
  chrome_browser_exe = os.getenv('CHROME_BROWSER_EXE', default_exe)
  if not os.access(chrome_browser_exe, os.X_OK):
    logging.fatal('cannot find chrome exe %s', chrome_browser_exe)
    sys.exit(-1)
  argv_new = [chrome_browser_exe]
  for a in argv[1:]:
    # NOTE: selenium work around to remove harmful quotes.
    if a.startswith('--user-data-dir='):
      a = a.replace('"', '')
    argv_new.append(a)
  argv_new += extra_args
  logging.info('launching chrome: %s', repr(argv_new))
  os.execvp(chrome_browser_exe, argv_new)


######################################################################
# Linux Specific Definitions and Functions
######################################################################
CHROME_LINUX_DEFAULT_EXE = '/opt/google/chrome/chrome'
CHROME_LINUX_EXTRA_ARGS  = ['--internal-nacl',
                            '--internal-pepper',
                            '--enable-gpu-plugin']

# Currently not used.
CHROME_LINUX_EXTRA_LD_LIBRARY_PATH = []
CHROME_LINUX_EXTRA_PATH = '/opt/google/chrome/'

def StartChromeLinux(argv):
  StartChrome(CHROME_LINUX_DEFAULT_EXE, argv, CHROME_LINUX_EXTRA_ARGS)


######################################################################
# Mac Specific Definitions and Functions
######################################################################
CHROME_MAC_DEFAULT_EXE = ('/Applications/Google Chrome.app/Contents/MacOS'
                          '/Google Chrome')
CHROME_MAC_EXTRA_ARGS  = ['--internal-nacl',
                          '--internal-pepper',
                          '--enable-gpu-plugin']
# Currently not used.
CHROME_MAC_EXTRA_LD_LIBRARY_PATH = []
CHROME_MAC_EXTRA_PATH = ''

def StartChromeMac(argv):
  StartChrome(CHROME_MAC_DEFAULT_EXE, argv, CHROME_MAC_EXTRA_ARGS)


######################################################################
# Main
######################################################################
def main(argv):
  logging.info('chrome wrapper started on %s', repr(UNAME))
  if UNAME[0] == 'Linux':
    StartChromeLinux(argv)
  elif UNAME[0] == 'Darwin':
    StartChromeMac(argv)
  else:
    logging.fatal('unsupport platform')

if __name__ == '__main__':
  sys.exit(main(sys.argv))
