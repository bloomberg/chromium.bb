#!/usr/bin/python2.4
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''The 'grit build' tool along with integration for this tool with the
SCons build system.
'''

import filecmp
import getopt
import os
import shutil
import sys
import types

from grit import grd_reader
from grit import util
from grit.tool import interface
from grit import shortcuts


def ParseDefine(define):
  '''Parses a define that is either like "NAME" or "NAME=VAL" and
  returns its components, using True as the default value.  Values of
  "1" and "0" are transformed to True and False respectively.
  '''
  parts = [part.strip() for part in define.split('=')]
  assert len(parts) >= 1
  name = parts[0]
  val = True
  if len(parts) > 1:
    val = parts[1]
  if val == "1": val = True
  elif val == "0": val = False
  return (name, val)


class RcBuilder(interface.Tool):
  '''A tool that builds RC files and resource header files for compilation.

Usage:  grit build [-o OUTPUTDIR] [-D NAME[=VAL]]*

All output options for this tool are specified in the input file (see
'grit help' for details on how to specify the input file - it is a global
option).

Options:

  -o OUTPUTDIR      Specify what directory output paths are relative to.
                    Defaults to the current directory.

  -D NAME[=VAL]     Specify a C-preprocessor-like define NAME with optional
                    value VAL (defaults to 1) which will be used to control
                    conditional inclusion of resources.

  -E NAME=VALUE     Set environment variable NAME to VALUE (within grit).

  -f FIRSTIDFILE    Path to a python file that specifies the first id of
                    value to use for resources.  Defaults to the file
                    resources_ids next to grit.py.  Set to an empty string
                    if you don't want to use a first id file.

  -w WHITELISTFILE  Path to a file containing the string names of the
                    resources to include.  Anything not listed is dropped.


Conditional inclusion of resources only affects the output of files which
control which resources get linked into a binary, e.g. it affects .rc files
meant for compilation but it does not affect resource header files (that define
IDs).  This helps ensure that values of IDs stay the same, that all messages
are exported to translation interchange files (e.g. XMB files), etc.
'''

  def ShortDescription(self):
    return 'A tool that builds RC files for compilation.'

  def Run(self, opts, args):
    self.output_directory = '.'
    first_id_filename = None
    whitelist_filenames = []
    (own_opts, args) = getopt.getopt(args, 'o:D:E:f:w:')
    for (key, val) in own_opts:
      if key == '-o':
        self.output_directory = val
      elif key == '-D':
        name, val = ParseDefine(val)
        self.defines[name] = val
      elif key == '-E':
        (env_name, env_value) = val.split('=')
        os.environ[env_name] = env_value
      elif key == '-f':
        first_id_filename = val
      elif key == '-w':
        whitelist_filenames.append(val)

    if len(args):
      print "This tool takes no tool-specific arguments."
      return 2
    self.SetOptions(opts)
    if self.scons_targets:
      self.VerboseOut('Using SCons targets to identify files to output.\n')
    else:
      self.VerboseOut('Output directory: %s (absolute path: %s)\n' %
                      (self.output_directory,
                       os.path.abspath(self.output_directory)))

    if whitelist_filenames:
      self.whitelist_names = set()
      for whitelist_filename in whitelist_filenames:
        self.VerboseOut('Using whitelist: %s\n' % whitelist_filename);
        whitelist_file = open(whitelist_filename)
        self.whitelist_names |= set(whitelist_file.read().strip().split('\n'))
        whitelist_file.close()

    self.res = grd_reader.Parse(opts.input, first_id_filename=first_id_filename,
                                debug=opts.extra_verbose, defines=self.defines)
    self.res.RunGatherers(recursive = True)
    self.Process()
    return 0

  def __init__(self):
    # Default file-creation function is built-in file().  Only done to allow
    # overriding by unit test.
    self.fo_create = file

    # key/value pairs of C-preprocessor like defines that are used for
    # conditional output of resources
    self.defines = {}

    # self.res is a fully-populated resource tree if Run()
    # has been called, otherwise None.
    self.res = None

    # Set to a list of filenames for the output nodes that are relative
    # to the current working directory.  They are in the same order as the
    # output nodes in the file.
    self.scons_targets = None

    # The set of names that are whitelisted to actually be included in the
    # output.
    self.whitelist_names = None


  # static method
  def AddWhitelistTags(start_node, whitelist_names):
    # Walk the tree of nodes added attributes for the nodes that shouldn't
    # be written into the target files (skip markers).
    from grit.node import include
    from grit.node import message
    for node in start_node.inorder():
      # Same trick data_pack.py uses to see what nodes actually result in
      # real items.
      if (isinstance(node, include.IncludeNode) or
          isinstance(node, message.MessageNode)):
        text_ids = node.GetTextualIds()
        # Mark the item to be skipped if it wasn't in the whitelist.
        if text_ids and not text_ids[0] in whitelist_names:
          node.SetWhitelistMarkedAsSkip(True)
  AddWhitelistTags = staticmethod(AddWhitelistTags)

  # static method
  def ProcessNode(node, output_node, outfile):
    '''Processes a node in-order, calling its formatter before and after
    recursing to its children.

    Args:
      node: grit.node.base.Node subclass
      output_node: grit.node.io.File
      outfile: open filehandle
    '''
    # See if the node should be skipped by a whitelist.
    # Note: Some Format calls have side effects, so Format is always called
    # and the whitelist is used to only avoid the output.
    should_write = not node.WhitelistMarkedAsSkip()

    base_dir = util.dirname(output_node.GetOutputFilename())

    try:
      formatter = node.ItemFormatter(output_node.GetType())
      if formatter:
        formatted = formatter.Format(node, output_node.GetLanguage(),
                                     begin_item=True, output_dir=base_dir)
        if should_write:
          outfile.write(formatted)
    except:
      print u"Error processing node %s" % unicode(node)
      raise

    for child in node.children:
      RcBuilder.ProcessNode(child, output_node, outfile)

    try:
      if formatter:
        formatted = formatter.Format(node, output_node.GetLanguage(),
                                     begin_item=False, output_dir=base_dir)
        if should_write:
          outfile.write(formatted)
    except:
      print u"Error processing node %s" % unicode(node)
      raise
  ProcessNode = staticmethod(ProcessNode)


  def Process(self):
    # Update filenames with those provided by SCons if we're being invoked
    # from SCons.  The list of SCons targets also includes all <structure>
    # node outputs, but it starts with our output files, in the order they
    # occur in the .grd
    if self.scons_targets:
      assert len(self.scons_targets) >= len(self.res.GetOutputFiles())
      outfiles = self.res.GetOutputFiles()
      for ix in range(len(outfiles)):
        outfiles[ix].output_filename = os.path.abspath(
          self.scons_targets[ix])
    else:
      for output in self.res.GetOutputFiles():
        output.output_filename = os.path.abspath(os.path.join(
          self.output_directory, output.GetFilename()))

    # If there are whitelisted names, tag the tree once up front, this way
    # while looping through the actual output, it is just an attribute check.
    if self.whitelist_names:
      self.AddWhitelistTags(self.res, self.whitelist_names)

    for output in self.res.GetOutputFiles():
      self.VerboseOut('Creating %s...' % output.GetFilename())

      # Microsoft's RC compiler can only deal with single-byte or double-byte
      # files (no UTF-8), so we make all RC files UTF-16 to support all
      # character sets.
      if output.GetType() in ('rc_header', 'resource_map_header',
          'resource_map_source', 'resource_file_map_source'):
        encoding = 'cp1252'
      elif output.GetType() in ('js_map_format', 'plist', 'plist_strings',
          'doc', 'json'):
        encoding = 'utf_8'
      else:
        # TODO(gfeher) modify here to set utf-8 encoding for admx/adml
        encoding = 'utf_16'

      # Make the output directory if it doesn't exist.
      outdir = os.path.split(output.GetOutputFilename())[0]
      if not os.path.exists(outdir):
        os.makedirs(outdir)
      # Write the results to a temporary file and only overwrite the original
      # if the file changed.  This avoids unnecessary rebuilds.
      outfile = self.fo_create(output.GetOutputFilename() + '.tmp', 'wb')

      if output.GetType() != 'data_package':
        outfile = util.WrapOutputStream(outfile, encoding)

      # Set the context, for conditional inclusion of resources
      self.res.SetOutputContext(output.GetLanguage(), self.defines)

      # TODO(joi) Handle this more gracefully
      import grit.format.rc_header
      grit.format.rc_header.Item.ids_ = {}

      # Iterate in-order through entire resource tree, calling formatters on
      # the entry into a node and on exit out of it.
      self.ProcessNode(self.res, output, outfile)
      outfile.close()

      # Now copy from the temp file back to the real output, but on Windows,
      # only if the real output doesn't exist or the contents of the file
      # changed.  This prevents identical headers from being written and .cc
      # files from recompiling (which is painful on Windows).
      if not os.path.exists(output.GetOutputFilename()):
        os.rename(output.GetOutputFilename() + '.tmp',
                  output.GetOutputFilename())
      else:
        # CHROMIUM SPECIFIC CHANGE.
        # This clashes with gyp + vstudio, which expect the output timestamp
        # to change on a rebuild, even if nothing has changed.
        #files_match = filecmp.cmp(output.GetOutputFilename(),
        #    output.GetOutputFilename() + '.tmp')
        #if (output.GetType() != 'rc_header' or not files_match
        #    or sys.platform != 'win32'):
        shutil.copy2(output.GetOutputFilename() + '.tmp',
                     output.GetOutputFilename())
        os.remove(output.GetOutputFilename() + '.tmp')

      self.VerboseOut(' done.\n')

    # Print warnings if there are any duplicate shortcuts.
    warnings = shortcuts.GenerateDuplicateShortcutsWarnings(
        self.res.UberClique(), self.res.GetTcProject())
    if warnings:
      print '\n'.join(warnings)

    # Print out any fallback warnings, and missing translation errors, and
    # exit with an error code if there are missing translations in a non-pseudo
    # and non-official build.
    warnings = (self.res.UberClique().MissingTranslationsReport().
        encode('ascii', 'replace'))
    if warnings and self.defines.get('_google_chrome', False):
      print warnings
    if self.res.UberClique().HasMissingTranslations():
      sys.exit(-1)
