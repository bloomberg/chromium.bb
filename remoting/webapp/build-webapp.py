#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a directory with with the unpacked contents of the remoting webapp.

The directory will contain a copy-of or a link-to to all remoting webapp
resources.  This includes HTML/JS and any plugin binaries. The script also
massages resulting files appropriately with host plugin data. Finally,
a zip archive for all of the above is produced.
"""

# Python 2.5 compatibility
from __future__ import with_statement

import os
import platform
import re
import shutil
import subprocess
import sys
import time
import zipfile

def findAndReplace(filepath, findString, replaceString):
  """Does a search and replace on the contents of a file."""
  oldFilename = os.path.basename(filepath) + '.old'
  oldFilepath = os.path.join(os.path.dirname(filepath), oldFilename)
  os.rename(filepath, oldFilepath)
  with open(oldFilepath) as input:
    with open(filepath, 'w') as output:
      for s in input:
        output.write(s.replace(findString, replaceString))
  os.remove(oldFilepath)


def createZip(zip_path, directory):
  """Creates a zipfile at zip_path for the given directory."""
  zipfile_base = os.path.splitext(os.path.basename(zip_path))[0]
  zip = zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED)
  for (root, dirs, files) in os.walk(directory):
    for f in files:
      full_path = os.path.join(root, f)
      rel_path = os.path.relpath(full_path, directory)
      zip.write(full_path, os.path.join(zipfile_base, rel_path))
  zip.close()


def buildWebApp(buildtype, mimetype, destination, zip_path, plugin, files,
                locales):
  """Does the main work of building the webapp directory and zipfile.

  Args:
    buildtype: the type of build ("Official" or "Dev")
    mimetype: A string with mimetype of plugin.
    destination: A string with path to directory where the webapp will be
                 written.
    zipfile: A string with path to the zipfile to create containing the
             contents of |destination|.
    plugin: A string with path to the binary plugin for this webapp.
    files: An array of strings listing the paths for resources to include
           in this webapp.
    locales: An array of strings listing locales, which are copied, along
             with their directory structure from the _locales directory down.
  """
  # Ensure a fresh directory.
  try:
    shutil.rmtree(destination)
  except OSError:
    if os.path.exists(destination):
      raise
    else:
      pass
  os.mkdir(destination, 0775)

  # Use symlinks on linux and mac for faster compile/edit cycle.
  #
  # On Windows Vista platform.system() can return 'Microsoft' with some
  # versions of Python, see http://bugs.python.org/issue1082
  # should_symlink = platform.system() not in ['Windows', 'Microsoft']
  #
  # TODO(ajwong): Pending decision on http://crbug.com/27185 we may not be
  # able to load symlinked resources.
  should_symlink = False

  # Copy all the files.
  for current_file in files:
    destination_file = os.path.join(destination, os.path.basename(current_file))
    destination_dir = os.path.dirname(destination_file)
    if not os.path.exists(destination_dir):
      os.makedirs(destination_dir, 0775)

    if should_symlink:
      # TODO(ajwong): Detect if we're vista or higher.  Then use win32file
      # to create a symlink in that case.
      targetname = os.path.relpath(os.path.realpath(current_file),
                                   os.path.realpath(destination_file))
      os.symlink(targetname, destination_file)
    else:
      shutil.copy2(current_file, destination_file)

  # Copy all the locales, preserving directory structure
  destination_locales = os.path.join(destination, "_locales")
  os.mkdir(destination_locales , 0775)
  chromium_locale_dir = "/_locales/"
  chrome_locale_dir = "/_locales.official/"
  for current_locale in locales:
    pos = current_locale.find(chromium_locale_dir)
    locale_len = len(chromium_locale_dir)
    if (pos == -1):
      pos = current_locale.find(chrome_locale_dir)
      locale_len = len(chrome_locale_dir)
    if (pos == -1):
      raise "Missing locales directory in " + current_locale
    subtree = current_locale[pos+locale_len:]
    pos = subtree.find("/")
    if (pos == -1):
      raise "Malformed locale: " + current_locale
    locale_id = subtree[:pos]
    messages = subtree[pos+1:]
    destination_dir = os.path.join(destination_locales, locale_id)
    destination_file = os.path.join(destination_dir, messages)
    os.mkdir(destination_dir, 0775)
    shutil.copy2(current_locale, destination_file)

  # Create fake plugin files to appease the manifest checker.
  # It requires that if there is a plugin listed in the manifest that
  # there be a file in the plugin with that name.
  names = [
    'remoting_host_plugin.dll',  # Windows
    'remoting_host_plugin.plugin',  # Mac
    'libremoting_host_plugin.ia32.so',  # Linux 32
    'libremoting_host_plugin.x64.so'  # Linux 64
  ]
  pluginName = os.path.basename(plugin)

  for name in names:
    if name != pluginName:
      path = os.path.join(destination, name)
      f = open(path, 'w')
      f.write("placeholder for %s" % (name))
      f.close()

  # Copy the plugin.
  pluginName = os.path.basename(plugin)
  newPluginPath = os.path.join(destination, pluginName)
  if os.path.isdir(plugin):
    # On Mac we have a directory.
    shutil.copytree(plugin, newPluginPath)
  else:
    shutil.copy2(plugin, newPluginPath)

  # Strip the linux build.
  if ((platform.system() == 'Linux') and (buildtype == 'Official')):
    subprocess.call(["strip", newPluginPath])

  # Add unique build numbers to manifest version.
  # For now, this is based on the system clock (seconds since 1/1/1970), since
  # a previous attempt (based on build/utils/lastchange.py) was failing on Mac.
  # TODO(lambroslambrou): Use the SVN revision number or an incrementing build
  # number (http://crbug.com/90110).
  timestamp = int(time.time())
  # Version string must be 1-4 numbers separated by dots, with each number
  # between 0 and 0xffff.
  version1 = timestamp / 0x10000
  version2 = timestamp % 0x10000
  findAndReplace(os.path.join(destination, 'manifest.json'),
                 'UNIQUE_VERSION',
                 '%d.%d' % (version1, version2))

  # Now massage files with our mimetype.
  findAndReplace(os.path.join(destination, 'plugin_settings.js'),
                 'HOST_PLUGIN_MIMETYPE',
                 mimetype)

  # Make the zipfile.
  createZip(zip_path, destination)


def main():
  if len(sys.argv) < 6:
    print ('Usage: build-webapp.py '
           '<build-type> <mime-type> <dst> <zip-path> <plugin> '
           '<other files...> --locales <locales...>')
    return 1

  reading_locales = False
  files = []
  locales = []
  for arg in sys.argv[6:]:
    if arg == "--locales":
      reading_locales = True;
    elif reading_locales:
      locales.append(arg)
    else:
      files.append(arg)

  buildWebApp(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5],
              files, locales)
  return 0


if __name__ == '__main__':
  sys.exit(main())
