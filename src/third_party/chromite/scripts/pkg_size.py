# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Package Size Reporting CLI entry point."""

from __future__ import print_function

import json
import sys

from chromite.lib import commandline
from chromite.lib import portage_util
from chromite.utils import metrics


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


def _get_parser():
  """Create an argument parser for this script."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--root', required=True, type='path',
                      help='Specify the rootfs to investigate.')
  parser.add_argument('--image-type',
                      help='Specify the type of image being investigated. '
                           'e.g. [base, dev, test]')
  parser.add_argument('--partition-name',
                      help='Specify the partition name. '
                           'e.g. [rootfs, stateful]')
  parser.add_argument('packages', nargs='*',
                      help='Names of packages to investigate. Must be '
                           'specified as category/package-version.')
  return parser


def generate_package_size_report(db, root, image_type, partition_name,
                                 installed_packages):
  """Collect package sizes and generate a report."""
  results = {}
  total_size = 0
  package_sizes = portage_util.GeneratePackageSizes(db, root,
                                                    installed_packages)
  timestamp = metrics.current_milli_time()
  for package_cpv, size in package_sizes:
    results[package_cpv] = size
    metrics.append_metrics_log(timestamp,
                               'package_size.%s.%s.%s' % (image_type,
                                                          partition_name,
                                                          package_cpv),
                               metrics.OP_GAUGE,
                               arg=size)
    total_size += size

  metrics.append_metrics_log(timestamp,
                             'total_size.%s.%s' % (image_type, partition_name),
                             metrics.OP_GAUGE,
                             arg=total_size)
  return {'root': root, 'package_sizes': results, 'total_size': total_size}


def main(argv):
  """Find and report approximate size info for a particular built package."""
  commandline.RunInsideChroot()

  parser = _get_parser()
  opts = parser.parse_args(argv)
  opts.Freeze()

  db = portage_util.PortageDB(root=opts.root)

  if opts.packages:
    installed_packages = portage_util.GenerateInstalledPackages(db, opts.root,
                                                                opts.packages)
  else:
    installed_packages = db.InstalledPackages()

  results = generate_package_size_report(db, opts.root, opts.image_type,
                                         opts.partition_name,
                                         installed_packages)
  print(json.dumps(results))
