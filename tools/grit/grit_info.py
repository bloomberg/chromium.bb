#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Tool to determine inputs and outputs of a grit file.
'''

import os
import posixpath
import types
import sys
from grit import grd_reader
from grit import util


def Outputs(filename):
  grd = grd_reader.Parse(filename)

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


def Inputs(filename):
  grd = grd_reader.Parse(filename, debug=False)
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

  # Add in the grit source files.  If one of these change, we want to re-run
  # grit.
  grit_root_dir = os.path.dirname(__file__)
  for root, dirs, filenames in os.walk(grit_root_dir):
    grit_src = [os.path.join(root, f) for f in filenames
                if f.endswith('.py') or f == 'resource_ids']
    files.extend(grit_src)

  return [f.replace('\\', '/') for f in files]


def main(argv):
  if len(argv) >= 3 and argv[1] == '--inputs':
    for f in argv[2:]:
      inputs = Inputs(f)
      # Include grd file as second input (works around gyp expecting it).
      inputs = [inputs[0], f] + inputs[1:]
      print '\n'.join(inputs)
    return 0
  if len(argv) >= 4 and argv[1] == '--outputs':
    for f in argv[3:]:
      outputs = [posixpath.join(argv[2], f) for f in Outputs(f)]
      print '\n'.join(outputs)
    return 0
  print 'USAGE: ./grit_info.py --inputs <grd-files>..'
  print '       ./grit_info.py --outputs <out-prefix> <grd-files>..'
  return 1


if __name__ == '__main__':
  sys.exit(main(sys.argv))
