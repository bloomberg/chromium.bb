#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
  Generates .msi from a .zip archive or an uppacked directory. The structure of
  the input archive or directory should look like this:

  +- archive.zip
     +- archive
        +- parameters.json

  The name of the archive and the top level directory in the archive must match.
  When an unpacked directory is used as the input "archive.zip/archive" should
  be passed via the command line.

  'parameters.json' specifies the parameters to be passed to candle/light and
  must have the following structure:

  {
    "defines": { "name": "value" },
    "extensions": [ "WixFirewallExtension.dll" ],
    "switches": [ '-nologo' ],
    "source": "chromoting.wxs",
    "bind_path": "files",
    "candle": { ... },
    "light": { ... }
  }

  "source" specifies the name of the input .wxs relative to
      "archive.zip/archive".
  "bind_path" specifies the path where to look for binary files referenced by
      .wxs relative to "archive.zip/archive".
"""

import copy
import json
from optparse import OptionParser
import os
import re
import subprocess
import sys
import zipfile

def extractZip(source, dest):
  """ Extracts |source| ZIP archive to |dest| folder returning |True| if
      successful."""
  archive = zipfile.ZipFile(source, 'r')
  for f in archive.namelist():
    target = os.path.normpath(os.path.join(dest, f))
    # Sanity check to make sure .zip uses relative paths.
    if os.path.commonprefix([target, dest]) != dest:
      print "Failed to unpack '%s': '%s' is not under '%s'" % (
          source, target, dest)
      return False

    # Create intermediate directories.
    target_dir = os.path.dirname(target)
    if not os.path.exists(target_dir):
      os.makedirs(target_dir)

    archive.extract(f, dest)
  return True

def merge(left, right):
  """ Merges to values. The result is:
    - if both |left| and |right| are dictionaries, they are merged recursively.
    - if both |left| and |right| are lists, the result is a list containing
        elements from both lists.
    - if both |left| and |right| are simple value, |right| is returned.
    - |TypeError| exception is raised if a dictionary or a list are merged with
        a non-dictionary or non-list correspondingly.
  """
  if isinstance(left, dict):
    if isinstance(right, dict):
      retval = copy.copy(left)
      for key, value in right.iteritems():
        if key in retval:
          retval[key] = merge(retval[key], value)
        else:
          retval[key] = value
      return retval
    else:
      raise TypeError("Error: merging a dictionary and non-dictionary value")
  elif isinstance(left, list):
    if isinstance(right, list):
      return left + right
    else:
      raise TypeError("Error: merging a list and non-list value")
  else:
    if isinstance(right, dict):
      raise TypeError("Error: merging a dictionary and non-dictionary value")
    elif isinstance(right, list):
      raise TypeError("Error: merging a dictionary and non-dictionary value")
    else:
      return right

quote_matcher_regex = re.compile(r'\s|"')
quote_replacer_regex = re.compile(r'(\\*)"')

def quoteArgument(arg):
  """Escapes a Windows command-line argument.

  So that the Win32 CommandLineToArgv function will turn the escaped result back
  into the original string.
  See http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
  ("Parsing C++ Command-Line Arguments") to understand why we have to do
  this.

  Args:
      arg: the string to be escaped.
  Returns:
      the escaped string.
  """

  def _Replace(match):
    # For a literal quote, CommandLineToArgv requires an odd number of
    # backslashes preceding it, and it produces half as many literal backslashes
    # (rounded down). So we need to produce 2n+1 backslashes.
    return 2 * match.group(1) + '\\"'

  if re.search(quote_matcher_regex, arg):
    # Escape all quotes so that they are interpreted literally.
    arg = quote_replacer_regex.sub(_Replace, arg)
    # Now add unescaped quotes so that any whitespace is interpreted literally.
    return '"' + arg + '"'
  else:
    return arg

def generateCommandLine(tool, source, dest, parameters):
  """Generates the command line for |tool|."""
  # Merge/apply tool-specific parameters
  params = copy.copy(parameters)
  if tool in parameters:
    params = merge(params, params[tool])

  wix_path = os.path.normpath(params.get('wix_path', ''))
  switches = [ os.path.join(wix_path, tool), '-nologo' ]

  # Append the list of defines and extensions to the command line switches.
  for name, value in params.get('defines', {}).iteritems():
    switches.append('-d%s=%s' % (name, value))

  for ext in params.get('extensions', []):
    switches += ('-ext', os.path.join(wix_path, ext))

  # Append raw switches
  switches += params.get('switches', [])

  # Append the input and output files
  switches += ('-out', dest, source)

  # Generate the actual command line
  #return ' '.join(map(quoteArgument, switches))
  return switches

def run(args):
  """ Constructs a quoted command line from the passed |args| list and runs it.
      Prints the exit code and output of the command if an error occurs."""
  command = ' '.join(map(quoteArgument, args))
  popen = subprocess.Popen(
      command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  out, _ = popen.communicate()
  if popen.returncode:
    print command
    for line in out.splitlines():
      print line
    print '%s returned %d' % (args[0], popen.returncode)
  return popen.returncode

def main():
  usage = "Usage: zip2msi [options] <input.zip> <output.msi>"
  parser = OptionParser(usage=usage)
  parser.add_option('--intermediate_dir', dest='intermediate_dir')
  parser.add_option('--wix_path', dest='wix_path')
  options, args = parser.parse_args()
  if len(args) != 2:
    parser.error("two positional arguments expected")
  parameters = dict(options.__dict__)

  parameters['basename'] = os.path.splitext(os.path.basename(args[0]))[0]
  if not options.intermediate_dir:
    parameters['intermediate_dir'] = os.path.normpath('.')

  # The script can handle both forms of input a directory with unpacked files or
  # a ZIP archive with the same files. In the latter case the archive should be
  # unpacked to the intermediate directory.
  intermediate_dir = os.path.normpath(parameters['intermediate_dir'])
  source_dir = None
  if os.path.isdir(args[0]):
    # Just use unpacked files from the supplied directory.
    source_dir = args[0]
  else:
    # Unpack .zip
    if not extractZip(args[0], intermediate_dir):
      return 1
    source_dir = '%(intermediate_dir)s\\%(basename)s' % parameters

  # Read parameters from 'parameters.json'.
  f = open(os.path.join(source_dir, 'parameters.json'))
  parameters = merge(parameters, json.load(f))
  f.close()

  if 'source' not in parameters:
    print "The source .wxs is not specified"
    return 1

  if 'bind_path' not in parameters:
    print "The binding path is not specified"
    return 1

  dest = args[1]
  source = os.path.join(source_dir, parameters['source'])

  #  Add the binding path to the light-specific parameters.
  bind_path = os.path.join(source_dir, parameters['bind_path'])
  parameters = merge(parameters, {'light': {'switches': ['-b', bind_path]}})

  # Run candle and light to generate the installation.
  wixobj = '%(intermediate_dir)s\\%(basename)s.wixobj' % parameters
  args = generateCommandLine('candle', source, wixobj, parameters)
  rc = run(args)
  if rc:
    return rc

  args = generateCommandLine('light', wixobj, dest, parameters)
  rc = run(args)
  if rc:
    return rc

  return 0

if __name__ == "__main__":
  sys.exit(main())

