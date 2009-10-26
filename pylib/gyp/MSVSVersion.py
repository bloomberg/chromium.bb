#!/usr/bin/python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Handle version information related to Visual Stuio."""

import os
import re
import subprocess
import sys


class VisualStudioVersion:
  """Information regarding a version of Visual Studio."""

  def __init__(self, short_name, description,
               solution_version, project_version):
    self.short_name = short_name
    self.description = description
    self.solution_version = solution_version
    self.project_version = project_version

  def ShortName(self):
    return self.short_name

  def Description(self):
    """Get the full description of the version."""
    return self.description

  def SolutionVersion(self):
    """Get the version number of the sln files."""
    return self.solution_version

  def ProjectVersion(self):
    """Get the version number of the vcproj files."""
    return self.project_version


def _RegistryGetValue(key, value):
  """Use reg.exe to read a paricular key.

  While ideally we might use the win32 module, we would like gyp to be
  python neutral, so for instance cygwin python lacks this module.

  Arguments:
    key: The registry key to read from.
    value: The particular value to read.
  Return:
    The contents there, or None for failure.
  """
  # Run reg.exe.
  cmd = [os.path.join(os.environ.get('WINDIR', ''), 'System32', 'reg.exe'),
         'query', key, '/v', value]
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  text = p.communicate()[0]
  # Require a successful return value.
  if p.returncode:
    return None
  # Extract value.
  match = re.search(r'REG_\w+[ ]+([^\r]+)\r\n', text)
  if not match:
    return None
  return match.group(1)


def _CreateVersion(name):
  if name == '2008':
    return VisualStudioVersion('2008',
                               'Visual Studio 2008',
                               solution_version='10.00',
                               project_version='9.00')
  elif name == '2005':
    return VisualStudioVersion('2005',
                               'Visual Studio 2005',
                               solution_version='9.00',
                               project_version='8.00')
  else:
    return None


def _DetectVisualStudioVersions():
  """Collect a list of installed visual studio version.

  Returns:
    A list of visual studio versions installed in descending order of
    usage preference.
    Base this on the registry and a quick check if devenv.exe exists.
    Only versions 8-9 are considered.
    Possibilities are:
      2005 - Visual Studio 2005 (8)
      2008 - Visual Studio 2008 (9)
  """
  version_to_year = { '8.0': '2005', '9.0': '2008' }
  versions = []
  for version in ['8.0', '9.0']:
    # Get the install dir for this version.
    key = r'HKLM\Software\Microsoft\VisualStudio\%s' % version
    path = _RegistryGetValue(key, 'InstallDir')
    if not path:
      continue
    # Check if there's anything actually there.
    if not os.path.exists(os.path.join(path, 'devenv.exe')):
      continue
    # Add this one.
    versions.append(_CreateVersion(version_to_year[version]))
  return versions


def SelectVisualStudioVersion(version='auto'):
  """Select which version of Visual Studio projects to generate.

  Arguments:
    version: Hook to allow caller to force a particular version (vs auto).
  Returns:
    An object representing a visual studio project format version.
  """
  # In auto mode, check environment variable for override.
  if version == 'auto':
    version = os.environ.get('GYP_MSVS_VERSION', 'auto')
  # In auto mode, pick the most preferred version present.
  if version == 'auto':
    versions = _DetectVisualStudioVersions()
    if not versions:
      # Default to 2005.
      return _CreateVersion('2005')
    return versions[0]
  # Convert version string into a version object.
  return _CreateVersion(version)
