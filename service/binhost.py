# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Binhost API interacts with Portage binhosts and Packages files."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils


def _ValidateBinhostConf(path, key):
  """Validates the binhost conf file defines only one environment variable.

  This function is effectively a sanity check that ensures unexpected
  configuration is not clobbered by conf overwrites.

  Args:
    path: Path to the file to validate.
    key: Expected binhost key.

  Raises:
    ValueError: If file defines != 1 environment variable.
  """
  if not os.path.exists(path):
    # If the conf file does not exist, e.g. with new targets, then whatever.
    return

  kvs = cros_build_lib.LoadKeyValueFile(path)

  if not kvs:
    raise ValueError(
        'Found empty .conf file %s when a non-empty one was expected.' % path)
  elif len(kvs) > 1:
    raise ValueError(
        'Conf file %s must define exactly 1 variable. '
        'Instead found: %r' % (path, kvs))
  elif key not in kvs:
    raise KeyError('Did not find key %s in %s' % (key, path))


def SetBinhost(target, key, uri, private=True):
  """Set binhost configuration for the given build target.

  A binhost is effectively a key (Portage env variable) pointing to a URL
  that contains binaries. The configuration is set in .conf files at static
  directories on a build target by build target (and host by host) basis.

  This function updates the .conf file by completely rewriting it.

  Args:
    target: The build target to set configuration for.
    key: The binhost key to set, e.g. POSTSUBMIT_BINHOST.
    uri: The new value for the binhost key, e.g. gs://chromeos-prebuilt/foo/bar.
    private: Whether or not the build target is private.

  Returns:
    Path to the updated .conf file.
  """
  conf_root = os.path.join(
      constants.SOURCE_ROOT,
      constants.PRIVATE_BINHOST_CONF_DIR if private else
      constants.PUBLIC_BINHOST_CONF_DIR, 'target')
  conf_file = '%s-%s.conf' % (target, key)
  conf_path = os.path.join(conf_root, conf_file)
  _ValidateBinhostConf(conf_path, key)
  osutils.WriteFile(conf_path, '%s="%s"' % (key, uri))
  return conf_path
