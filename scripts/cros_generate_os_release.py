# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Combine os-release.d fragments into /etc/os-release."""

from __future__ import print_function

import os

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import osutils


def GenerateOsRelease(root):
  """Adds contents of /etc/os-release.d into /etc/os-release

  Args:
    root: path to the root directory where os-release should be genereated.
  """
  os_release_path = os.path.join(root, 'etc', 'os-release')
  os_released_path = os.path.join(root, 'etc', 'os-release.d')

  if not os.path.isdir(os_released_path):
    # /etc/os-release.d does not exist, no need to regenerate /etc/os-release.
    return

  mapping = {}
  if os.path.exists(os_release_path):
    content = osutils.ReadFile(os_release_path)

    for line in content.splitlines():
      if line.startswith('#'):
        continue

      key_value = line.split('=', 1)
      if len(key_value) != 2:
        cros_build_lib.Die('Malformed line in /etc/os-release')

      mapping[key_value[0]] = key_value[1].strip()

  for filepath in os.listdir(os_released_path):
    key = os.path.basename(filepath)
    if key in mapping:
      cros_build_lib.Die('key %s defined in /etc/os-release.d but already '
                         'defined in /etc/os-release.' % key)
    mapping[key] = osutils.ReadFile(os.path.join(os_released_path,
                                                 filepath)).strip('\n')

  osrelease_content = '\n'.join([k + '=' + mapping[k] for k in mapping])
  osrelease_content += '\n'
  osutils.WriteFile(os_release_path, osrelease_content)


def main(argv):
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--root', type='path', required=True,
                      help='sysroot of the board')
  options = parser.parse_args(argv)
  options.Freeze()

  GenerateOsRelease(options.root)
