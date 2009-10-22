#!/usr/bin/python2.4
# Copyright 2008 Google Inc.
# All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#   * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#   * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# 'AS IS' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""WiX tool for SCons.

Acknowledgement: Rob Mensching's code sample at
http://www.scons.org/wiki/WiX_Tool provided invaluable insight
into how to write this sort of thing. Rob's code is a little
less lazy than mine--he parses the .wxs file directly, while
I just run the Candle in preprocessor mode and capture filenames
with a regex. I think his method is faster but mine may be
a little more robust.
"""

import os
import re
import subprocess
import xml.dom.minidom
import SCons.Defaults
import SCons.Scanner
import uuid


def WixHarvestScanner(node, env, path):
  """Dependency scanner for the wix_harvest function.

  Builds a list of all the files in <path>.

  There's some duplicated functionality between this
  and the main wix_harvest function, but I don't see a
  great way to factor it out just now.

  Note: this scanner duplicates the documented behavior
  of SCons.DirScanner. I say 'documented' behavior because
  as near as I can tell, DirScanner isn't working right. It
  may be that it doesn't work correctly on Windows systems.
  In any case, if DirScanner can be shown to work, this
  function would be redundant.

  Args:
    node: the SCons node to scan
    env:  the current SCons environment
    path: unused

  Returns:
    A list of files.
  """

  path = env.subst(node.abspath)

  depends = []

  for d, unused_s, files in os.walk(path, topdown=True):
    for f in files:
      depends.append(os.path.join(d, f))

  print depends
  return depends


def WixHarvest(target, source, env):
  """Adds multiple files and directories to a WiX project.

  Searches all of the subdirectories under source, finds all files,
  and adds them to a WiX-compatible XML fragment saved at 'target'

  Arguments:
    target: output filename
    source: path to source directory. Must be a directory, not a file.
    env:    SCons environment for this build

  Returns:
    0 on success.
  """

  path = env.subst(str(source[0].abspath))
  relpath = os.path.normpath(str(source[0]))

  # KLUDGE: MSI requires a short name for each file, even though
  # we don't actually support installation on systems that don't
  # support long filenames. WiX 3.0 solves this issue but WiX 2
  # doesn't. We could do what WiX 3 does and hash each filename,
  # but since the names only need to be unique within this document,
  # it seems simpler to just assign a monotonically increasing
  # value to each file.
  fileid = 0

  # Create an element tree to hold the structured data
  doc = xml.dom.minidom.Document()
  root = doc.createElementNS(
      'http://schemas.microsoft.com/wix/2003/01/wi',
      'Wix')

  doc.appendChild(root)
  fragment = doc.createElement('Fragment')
  root.appendChild(fragment)

  # There are two top level nodes: a directory reference
  # and a component group. The first defines the layout,
  # the second one is necessary because WiX is redundant.
  topname = os.path.splitext(os.path.basename(str(target[0])))[0]

  top = doc.createElement('DirectoryRef')
  top.setAttribute('Id', topname)
  fragment.appendChild(top)

  group = doc.createElement('ComponentGroup')
  group.setAttribute('Id', topname)
  fragment.appendChild(group)

  nodes = dict({path: top})

  # Recursively walk the directories in path
  for (directory, subdirs, files) in os.walk(path, topdown=True):
    current = nodes[directory]
    # create component children for this directory. Each file
    # gets its own component, as suggested # by the WiX docs.
    # Every component needs a UUID, which we're generating
    # based on the relative path of the component in order to
    # minimize the likelihood of the UUID changing from build to
    # build. If it does change it's not the end of the world, but
    # it would make it more difficult to do incremental patches
    # using msiexec.

    for filename in files:
      fileid += 1
      fileguid = uuid.uuid3(uuid.NAMESPACE_URL,
                            relpath)

      component_id = 'Component' + str(fileid)
      component = doc.createElement ('Component')
      component.setAttribute('Id', component_id)
      component.setAttribute('Guid', str(fileguid))
      current.appendChild(component)
      filenode = doc.createElement('File')
      filenode.setAttribute('Id', 'File' + str(fileid))
      filenode.setAttribute('DiskId', '1')
      filenode.setAttribute('LongName', filename)
      filenode.setAttribute('Name', str(fileid))
      filenode.setAttribute('Source', os.path.join(directory, filename))
      component.appendChild(filenode)

      # create a reference to this component in the component group
      ref = doc.createElement('ComponentRef')
      ref.setAttribute('Id', component_id)
      group.appendChild(ref)

    # create a directory node for each subdirectory.
    for subdir in subdirs:
      fileid += 1
      subnode = doc.createElement('Directory')
      subnode.setAttribute('Id', 'Dir' + str(fileid))
      subnode.setAttribute('LongName', subdir)
      subnode.setAttribute('Name', str(fileid))
      current.appendChild(subnode)
      # remember this node so we can add its children later
      subdir_path = os.path.join(directory, subdir)
      nodes[subdir_path] = subnode
  outfile = open(str(target[0].abspath), 'w')
  outfile.write(doc.toprettyxml())
  outfile.close()

  return 0


def WixDepends(src_path, candle_path, wixflags):
  """Generates a list of dependencies for the given WiX file.

  The algorithm is fairly naive--it assumes that all 'Source'
  attributes represent dependencies--but it seems to work ok
  for our relatively simple needs.

  Arguments:
    src_path:     full path to WiX (.wxs) source file
    candle_path:  full path to candle.exe
    wixflags:     flags to pass to WiX preprocessor

  Returns:
    list of file dependencies
  """

  # Preprocess the .wxs file using candle.exe, the WiX
  # xml processor
  candle_pp_pipe = subprocess.Popen(
      [
          candle_path,
          src_path,
          wixflags,
          '-p',
      ],
      stdout=subprocess.PIPE)

  candle_pp_output = candle_pp_pipe.communicate()[0]

  # Process the output through a regex. We're looking for
  # anything with the pattern Source='<filename>', and
  # we want to accept either double or single quotes.
  sources_regex = re.compile(
      r"[S|s]ource=(?P<quote>[\"|'])(?P<filename>.*)(?P=quote)")

  matches = sources_regex.findall(candle_pp_output)

  # The matches are returned as a list of tuples.
  # This code changes it into a tuple of lists. We
  # keep the 2nd list and throw the first away, since
  # the first list contains only the captured quote
  # fields.
  files = []

  if matches:
    files = list(zip(*matches)[1])

  return files


def WixScanner(node, env, unused_path):
  """Simple scanner for WiX (.wxs) files.

  Arguments:
    node:         the SCons node to scan
    env:          the current SCons environment
    unused_path:  unused

  Returns:
    file list
  """

  files = WixDepends(
      str(node),
      env.subst('$WIX_CANDLE_PATH'),
      env.subst('$WIXFLAGS'))

  return files


def generate(env):
  """SCons entry function.

  The name of this function is required by SCons and cannot be changed,
  no matter what the style guide or GPyLint may say.

  Arguments:
    env:    The SCons environment in which this script is executing
  """
  # set the default path to the WiX toolkit
  env.SetDefault(
      WIX_PATH=os.path.join (
          '${SOURCE_ROOT}',
          'third_party',
          'wix_2_0_4221',
          'files'))

  # define the paths to the candle and light executables,
  # and set them into the environment (we'll need them later)
  candle_exe = os.path.join('${WIX_PATH}', 'candle.exe')
  env.SetDefault(WIX_CANDLE_PATH=candle_exe)
  light_exe = os.path.join('${WIX_PATH}', 'light.exe')
  env.SetDefault(WIX_LIGHT_PATH=light_exe)

  # Create a dependency scanner for .wxs files and add it
  # to the current environment
  scanner = SCons.Scanner.Scanner(
      function=WixScanner,
      skeys=['.wxs'])

  env.Append(SCANNERS=scanner)

  # WiX works in two stages:
  #  1. Candle.exe preprocesses the WiX XML into the tedious and
  #    verbose XML relational database format that the MSI tools
  #    expect. The output is a .wixobj file.
  #  2. Light.exe processes the .wixobj and builds the final MSI
  #    package.
  #
  # We define both stages separately, but only add the final 'Light'
  # stage to the environment. The 'Candle' stage doesn't need to go
  # directly into the environment, because it's linked to the 'Light'
  # stage via the src_builder argument.
  candle = SCons.Builder.Builder(
      action=candle_exe + ' $SOURCE $WIXFLAGS -out $TARGET',
      src_suffix=['.wxs'],
      suffix='.wixobj',
      ensure_suffix=True,
      single_source=True)

  light = SCons.Builder.Builder(
      action=light_exe
      + ' $SOURCES $WIXLIBS $WIXFLAGS -loc $WIXLOC -out $TARGET',
      src_suffix=['.wixobj'],
      single_source=False,
      src_builder=candle)

  # Add the Light builder to the environment, but call it WixPackage
  # since 'Light' isn't likely to have meaning for anyone who's not
  # already steeped in WiX lore. Plus, it's a replacement for the
  # built-in SCons Package builder, which also creates MSI files
  # but doesn't have the same flexibility as WiX.
  env.Append(BUILDERS={'WixPackage': light})

  # Add a builder that harvests files from a directory to build a
  # WiX script
  harvest = SCons.Builder.Builder(
      action=WixHarvest,
      source_factory=env.Dir,
      single_source=True)

  env.Append(BUILDERS={'Harvest': harvest})

