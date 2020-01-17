# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dumps a JSON file that describes objects installed for the given boards.

e.g.,

{
  'bob': {
    'chromeos-base/chromeos-chrome': [
      '/opt/google/chrome/chrome',
      '/opt/google/chrome/libchrome.so'
    ]
  }
}
"""

from __future__ import print_function

import json
import os
import os.path

from chromite.lib import commandline, cros_build_lib, portage_util


def get_all_package_objects(board):
  """Given a board, returns a dict of {package_name: [objects_in_package]}

  `objects_in_package` is specifically talking about objects of type `obj`
  which were installed by the given package. In other words, this will
  enumerate all regular files (e.g., excluding directories and symlinks)
  installed by a package.

  This dict comprises all packages currently installed on said board.
  """
  db = portage_util.PortageDB(root=os.path.join('/build', board))

  result = {}
  for package in db.InstalledPackages():
    objects = [
        '/' + path for typ, path in package.ListContents() if typ == package.OBJ
    ]
    objects.sort()
    result['%s/%s' % (package.category, package.package)] = objects
  return result


def main(argv):
  cros_build_lib.AssertInsideChroot()

  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--output', required=True, help='File to write results to')
  parser.add_argument('board', nargs='+')
  opts = parser.parse_args(argv)

  output = opts.output
  if output == '-':
    output = '/dev/stdout'

  results = {x: get_all_package_objects(x) for x in opts.board}
  with open(output, 'w') as f:
    json.dump(results, f)
