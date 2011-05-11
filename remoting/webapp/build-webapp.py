#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copies the remoting webapp resources and host plugin into a single directory.

import os
import shutil
import sys

# Do not copy git and svn files into the build.
IGNORE_PATTERNS = ('.git', '.svn')

if len(sys.argv) != 4:
  print 'Usage: build-webapp.py <webapp-resource-dir> <host-plugin> <dst>'
  sys.exit(1)

resources = sys.argv[1]
plugin = sys.argv[2]
destination = sys.argv[3]

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
templatePath = os.path.join(destination, 'manifest.json.template')
manifestPath = os.path.join(destination, 'manifest.json')
input = open(templatePath)
output = open(manifestPath, 'w')
for s in input:
  output.write(s.replace('HOST_PLUGIN_NAME', pluginName))
input.close()
output.close()
os.remove(templatePath)
