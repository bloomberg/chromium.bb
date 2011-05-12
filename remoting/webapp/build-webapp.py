#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copies the remoting webapp resources and host plugin into a single directory.
# Massages the files appropriately with host plugin data.

# Python 2.5 compatibility
from __future__ import with_statement

import os
import shutil
import sys

def findAndReplace( filepath, findString, replaceString ):
  oldFilename = os.path.basename(filepath) + '.old'
  oldFilepath = os.path.join(os.path.dirname(filepath), oldFilename)
  os.rename(filepath, oldFilepath)
  with open(oldFilepath) as input:
    with open(filepath, 'w') as output:
      for s in input:
        output.write(s.replace(findString, replaceString))
  os.remove(oldFilepath)

# Temporary hack to work around fact that some build machines don't have
# python 2.6
# TODO(dmaclach): fix this
try:
  from shutil import ignore_patterns
except Exception:
  exit(0)

# Do not copy git and svn files into the build.
IGNORE_PATTERNS = ('.git', '.svn')

if len(sys.argv) != 5:
  print 'Usage: build-webapp.py <mime-type> <webapp-dir> <host-plugin> <dst>'
  sys.exit(1)

mimetype = sys.argv[1]
resources = sys.argv[2]
plugin = sys.argv[3]
destination = sys.argv[4]

try:
  shutil.rmtree(destination)
except OSError:
  if os.path.exists(destination):
    raise
  else:
    pass

shutil.copytree(resources,
                destination,
                ignore=shutil.ignore_patterns(*IGNORE_PATTERNS))
pluginName = os.path.basename(plugin)
newPluginPath = os.path.join(destination, pluginName)

# On Mac we have a directory
if os.path.isdir(plugin):
  shutil.copytree(plugin,
                  newPluginPath,
                  ignore=shutil.ignore_patterns(*IGNORE_PATTERNS))
else:
  shutil.copy2(plugin, newPluginPath)

# Now massage the manifest to the right plugin name
findAndReplace(os.path.join(destination,'manifest.json'),
               '"PLUGINS": "PLACEHOLDER"',
                '"plugins": [\n    { "path": "' + pluginName +'" }\n  ]')

# Now massage files with our mimetype
findAndReplace(os.path.join(destination,'remoting.js'),
               'HOST_PLUGIN_MIMETYPE',
               mimetype)
