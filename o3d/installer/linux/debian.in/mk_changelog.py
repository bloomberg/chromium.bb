#!/usr/bin/python2.4
#
# Copyright 2010 Google Inc. All Rights Reserved.

'''Makes a debian/changelog file from a template'''

__author__ = 'jeffbailey@google.com (Jeff Bailey)'

import optparse
import os
import subprocess
import sys

def GetOptionParser():
  '''Constructs and configures an option parser for this script.

  Returns:
    An initalised OptionParser instance.
  '''
  parser = optparse.OptionParser('usage: %prog [options]')
  parser.add_option('-v', '--version', dest='version',
                    help='O3D version number')
  parser.add_option('-i', '--in', dest='infile',
                    help='Input file')
  parser.add_option('-o', '--out', dest='outfile',
                    help='Output file')
  return parser

def Main():
  parser = GetOptionParser()
  (options, args) = parser.parse_args()

  version = options.version
  outfile = options.outfile
  infile = options.infile

  if not version:
    parser.error('Missing --version')

  if not outfile:
    parser.error('Missing --out')

  if not infile:
    parser.error('Missing --in')

  deb_date = os.popen('date -R').read().rstrip()
  sed_exp = "-e 's/VERSION/%s/' -e 's/DATE/%s/'" % (version, deb_date)

  sed_cmd = '/bin/sed %s %s >%s' % (sed_exp, infile, outfile)

  print sed_cmd
  os.system(sed_cmd)

if __name__ == '__main__':
  sys.exit(Main())
