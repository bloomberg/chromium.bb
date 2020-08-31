# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enumerates all Chrome OS packages that are marked as `hot`.

Dumps results as a list of package names to a JSON file. Hotness is
determined by statically analyzing an ebuild.

Primarily intended for use by the Chrome OS toolchain team.
"""

from __future__ import print_function

import json
import os
import sys

from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.lib import portage_util


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


def is_ebuild_marked_hot(ebuild_path):
  with open(ebuild_path) as f:
    # The detection of this is intentionally super simple.
    #
    # It's important to note that while this is a function, we also use it in
    # comments in packages that are forcibly optimized for speed in some other
    # way, like chromeos-chrome.
    return any('cros_optimize_package_for_speed' in line for line in f)


def enumerate_package_ebuilds():
  """Determines package -> ebuild mappings for all packages.

  Yields a series of (package_path, package_name, [path_to_ebuilds]). This may
  yield the same package name multiple times if it's available in multiple
  overlays.
  """
  for overlay in portage_util.FindOverlays(overlay_type='both'):
    logging.debug('Found overlay %s', overlay)

    # Note that portage_util.GetOverlayEBuilds can't be used here, since that
    # specifically only searches for cros_workon candidates. We care about
    # everything we can possibly build.
    for dir_path, dir_names, file_names in os.walk(overlay):
      ebuilds = [x for x in file_names if x.endswith('.ebuild')]
      if not ebuilds:
        continue

      # os.walk directly uses `dir_names` to figure out what to walk next. If
      # there are ebuilds here, walking any lower is a waste, so don't do it.
      del dir_names[:]

      ebuild_dir = os.path.basename(dir_path)
      ebuild_parent_dir = os.path.basename(os.path.dirname(dir_path))
      package_name = '%s/%s' % (ebuild_parent_dir, ebuild_dir)
      yield dir_path, package_name, ebuilds


def main(argv):
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--output', required=True)
  opts = parser.parse_args(argv)

  ebuilds_found = 0
  packages_found = 0
  merged_packages = 0
  mappings = {}
  for package_dir, package, ebuilds in enumerate_package_ebuilds():
    packages_found += 1
    ebuilds_found += len(ebuilds)
    logging.debug('Found package %r in %r with ebuilds %r', package,
                  package_dir, ebuilds)

    is_marked_hot = any(
        is_ebuild_marked_hot(os.path.join(package_dir, x)) for x in ebuilds)
    if is_marked_hot:
      logging.debug('Package is marked as hot')
    else:
      logging.debug('Package is not marked as hot')

    if package in mappings:
      logging.warning('Multiple entries found for package %r; merging', package)
      merged_packages += 1
      mappings[package] = is_marked_hot or mappings[package]
    else:
      mappings[package] = is_marked_hot

  hot_packages = sorted(
      package for package, is_hot in mappings.items() if is_hot)

  logging.info('%d ebuilds found', ebuilds_found)
  logging.info('%d packages found', packages_found)
  logging.info('%d packages merged', merged_packages)
  logging.info('%d hot packages found, total', len(hot_packages))

  with open(opts.output, 'w') as f:
    json.dump(hot_packages, f)
