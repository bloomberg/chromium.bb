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

  # Add in the grit source files.  If one of these change, we want to re-run
  # grit.
  grit_root_dir = os.path.dirname(__file__)
  for root, dirs, filenames in os.walk(grit_root_dir):
    grit_src = [os.path.join(root, f) for f in filenames
                if f.endswith('.py') or f == 'resource_ids']
    files.extend(grit_src)

  return [f.replace('\\', '/') for f in files]


def PrintUsage():
  print 'USAGE: ./grit_info.py --inputs [-D foo] <grd-files>..'
  print '       ./grit_info.py --outputs [-D foo] <out-prefix> <grd-files>..'


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

  if not len(args):
    return None

  defines = {}
  for define in options.defines:
    defines[define] = 1

  if options.inputs:
    for filename in args:
      inputs = Inputs(filename, defines)
      # Include grd file as second input (works around gyp expecting it).
      inputs = [inputs[0], filename] + inputs[1:]
      if options.whitelist_files:
        inputs.extend(options.whitelist_files)
      return '\n'.join(inputs)
  elif options.outputs:
    if len(args) < 2:
      return None

    for f in args[1:]:
      outputs = [posixpath.join(args[0], f) for f in Outputs(f, defines)]
      return '\n'.join(outputs)
  else:
    return None


def main(argv):
  result = DoMain(argv[1:])
  if result == None:
    PrintUsage()
    return 1
  print result
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
