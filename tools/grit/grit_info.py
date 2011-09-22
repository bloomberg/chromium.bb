#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Tool to determine inputs and outputs of a grit file.
'''

import optparse
import os
import posixpath
import types
import sys

from grit import grd_reader
from grit import util

##############################################################################
# os.path.relpath is python 2.6 only. Some bots still run 2.5 only, so I took
# the relpath implementation from the python source.
# TODO(thakis): Once we're on 2.6 everywhere, remove this and use
# os.path.relpath directly.

# http://docs.python.org/license.html
# PSF LICENSE AGREEMENT FOR PYTHON 2.7.1
#
# 1. This LICENSE AGREEMENT is between the Python Software Foundation ("PSF"),
# and the Individual or Organization ("Licensee") accessing and otherwise using
# Python 2.7.1 software in source or binary form and its associated
# documentation.
#
# 2. Subject to the terms and conditions of this License Agreement, PSF hereby
# grants Licensee a nonexclusive, royalty-free, world-wide license to reproduce,
# analyze, test, perform and/or display publicly, prepare derivative works,
# distribute, and otherwise use Python 2.7.1 alone or in any derivative version,
# provided, however, that PSF's License Agreement and PSF's notice of copyright,
# i.e., "Copyright c 2001-2010 Python Software Foundation; All Rights Reserved"
# are retained in Python 2.7.1 alone or in any derivative version prepared by
# Licensee.
#
# 3. In the event Licensee prepares a derivative work that is based on or
# incorporates Python 2.7.1 or any part thereof, and wants to make the
# derivative work available to others as provided herein, then Licensee hereby
# agrees to include in any such work a brief summary of the changes made to
# Python 2.7.1.
#
# 4. PSF is making Python 2.7.1 available to Licensee on an "AS IS" basis. PSF
# MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
# BUT NOT LIMITATION, PSF MAKES NO AND DISCLAIMS ANY REPRESENTATION OR WARRANTY
# OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF
# PYTHON 2.7.1 WILL NOT INFRINGE ANY THIRD PARTY RIGHTS.
#
# 5.1 PSF SHALL NOT BE LIABLE TO LICENSEE OR ANY OTHER USERS OF PYTHON 2.7.1 FOR
# ANY INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES OR LOSS AS A RESULT OF
# MODIFYING, DISTRIBUTING, OR OTHERWISE USING PYTHON 2.7.1, OR ANY DERIVATIVE
# THEREOF, EVEN IF ADVISED OF THE POSSIBILITY THEREOF.
#
# 6. This License Agreement will automatically terminate upon a material breach
# of its terms and conditions.
#
# 7. Nothing in this License Agreement shall be deemed to create any
# relationship of agency, partnership, or joint venture between PSF and
# Licensee. This License Agreement does not grant permission to use PSF
# trademarks or trade name in a trademark sense to endorse or promote products
# or services of Licensee, or any third party.
#
# 8. By copying, installing or otherwise using Python 2.7.1, Licensee agrees to
# be bound by the terms and conditions of this License Agreement.

# http://svn.python.org/view/python/trunk/Lib/genericpath.py?view=markup
def commonprefix(m):
    "Given a list of pathnames, returns the longest common leading component"
    if not m: return ''
    s1 = min(m)
    s2 = max(m)
    for i, c in enumerate(s1):
        if c != s2[i]:
            return s1[:i]
    return s1


# http://svn.python.org/view/python/trunk/Lib/posixpath.py?view=markup
def relpath(path, start=os.path.curdir):
    """Return a relative version of a path"""

    if not path:
        raise ValueError("no path specified")

    start_list = os.path.abspath(start).split(os.path.sep)
    path_list = os.path.abspath(path).split(os.path.sep)

    # Work out how much of the filepath is shared by start and path.
    i = len(commonprefix([start_list, path_list]))

    rel_list = [os.path.pardir] * (len(start_list)-i) + path_list[i:]
    if not rel_list:
        return os.path.curdir
    return os.path.join(*rel_list)
##############################################################################


class WrongNumberOfArguments(Exception):
  pass


def Outputs(filename, defines):
  grd = grd_reader.Parse(
      filename, defines=defines, tags_to_ignore=set(['messages']))

  target = []
  lang_folders = {}
  # Add all explicitly-specified output files
  for output in grd.GetOutputFiles():
    path = output.GetFilename()
    target.append(path)

    if path.endswith('.h'):
      path, filename = os.path.split(path)
    if output.attrs['lang']:
      lang_folders[output.attrs['lang']] = os.path.dirname(path)

  # Add all generated files, once for each output language.
  for node in grd:
    if node.name == 'structure':
      # TODO(joi) Should remove the "if sconsdep is true" thing as it is a
      # hack - see grit/node/structure.py
      if node.HasFileForLanguage() and node.attrs['sconsdep'] == 'true':
        for lang in lang_folders:
          path = node.FileForLanguage(lang, lang_folders[lang],
                                      create_file=False,
                                      return_if_not_generated=False)
          if path:
            target.append(path)

  return [t.replace('\\', '/') for t in target]


def GritSourceFiles():
  files = []
  grit_root_dir = relpath(os.path.dirname(__file__), os.getcwd())
  for root, dirs, filenames in os.walk(grit_root_dir):
    grit_src = [os.path.join(root, f) for f in filenames
                if f.endswith('.py')]
    files.extend(grit_src)
  # TODO(joi@chromium.org): Once we switch to specifying the
  # resource_ids file via a .grd attribute, it should be considered an
  # input of grit and this bit should no longer be necessary.
  default_resource_ids = relpath(
      os.path.join(grit_root_dir, '..', 'gritsettings', 'resource_ids'),
      os.getcwd())
  if os.path.exists(default_resource_ids):
    files.append(default_resource_ids)
  return files


def Inputs(filename, defines):
  grd = grd_reader.Parse(
      filename, debug=False, defines=defines, tags_to_ignore=set(['messages']))
  files = []
  for node in grd:
    if (node.name == 'structure' or node.name == 'skeleton' or
        (node.name == 'file' and node.parent and
         node.parent.name == 'translations')):
      files.append(node.GetFilePath())
    elif node.name == 'include':
      # Only include files that we actually plan on using.
      if node.SatisfiesOutputCondition():
        files.append(node.FilenameToOpen())
        # If it's a flattened node, grab inlined resources too.
        if node.attrs['flattenhtml'] == 'true':
          files.extend(node.GetHtmlResourceFilenames())

  return files


def PrintUsage():
  print 'USAGE: ./grit_info.py --inputs [-D foo] <grd-file>'
  print '       ./grit_info.py --outputs [-D foo] <out-prefix> <grd-file>'


def DoMain(argv):
  parser = optparse.OptionParser()
  parser.add_option("--inputs", action="store_true", dest="inputs")
  parser.add_option("--outputs", action="store_true", dest="outputs")
  parser.add_option("-D", action="append", dest="defines", default=[])
  # grit build also supports '-E KEY=VALUE', support that to share command
  # line flags.
  parser.add_option("-E", action="append", dest="build_env", default=[])
  parser.add_option("-w", action="append", dest="whitelist_files", default=[])

  options, args = parser.parse_args(argv)

  defines = {}
  for define in options.defines:
    defines[define] = 1

  if options.inputs:
    if len(args) > 1:
      raise WrongNumberOfArguments("Expected 0 or 1 arguments for --inputs.")

    inputs = []
    if len(args) == 1:
      filename = args[0]
      inputs = Inputs(filename, defines)

    # Add in the grit source files.  If one of these change, we want to re-run
    # grit.
    inputs.extend(GritSourceFiles())
    inputs = [f.replace('\\', '/') for f in inputs]

    if len(args) == 1:
      # Include grd file as second input (works around gyp expecting it).
      inputs = [inputs[0], args[0]] + inputs[1:]
    if options.whitelist_files:
      inputs.extend(options.whitelist_files)
    return '\n'.join(inputs)
  elif options.outputs:
    if len(args) != 2:
      raise WrongNumberOfArguments("Expected exactly 2 arguments for --ouputs.")

    prefix, filename = args
    outputs = [posixpath.join(prefix, f) for f in Outputs(filename, defines)]
    return '\n'.join(outputs)
  else:
    raise WrongNumberOfArguments("Expected --inputs or --outputs.")


def main(argv):
  try:
    result = DoMain(argv[1:])
  except WrongNumberOfArguments, e:
    PrintUsage()
    print e
    return 1
  print result
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
