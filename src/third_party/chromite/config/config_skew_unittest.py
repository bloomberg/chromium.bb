# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for checking skew between old and new config worlds."""

from __future__ import print_function

import json
import os

from chromite.config import chromeos_config
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import osutils

BUILDER_CONFIG_FILENAME = os.path.join(
    constants.SOURCE_ROOT, 'infra/config/generated/builder_configs.cfg')

class ConfigSkewTest(cros_test_lib.TestCase):
  """Tests checking if new config and legacy config are in sync."""

  def __init__(self, *args, **kwargs):
    super(ConfigSkewTest, self).__init__(*args, **kwargs)
    self.new_configs = json.loads(osutils.ReadFile(BUILDER_CONFIG_FILENAME))
    self.old_configs = chromeos_config.GetConfig()

  def _get_new_config(self, name):
    for config in self.new_configs["builderConfigs"]:
      if config["id"]["name"] == name:
        return config
    return None

  def _get_old_config(self, name):
    return self.old_configs[name]

  def _to_utf8(self, strings):
    return [string.decode("UTF-8") for string in strings]

  @cros_test_lib.ConfigSkewTest()
  def testPostsubmitBuildTargets(self):
    master_postsubmit_config = self._get_old_config("master-postsubmit")
    postsubmit_orchestrator_config = self._get_new_config(
        "postsubmit-orchestrator")

    master_postsubmit_children = self._to_utf8(
        master_postsubmit_config.slave_configs)
    postsubmit_orchestrator_children = postsubmit_orchestrator_config[
        "orchestrator"]["children"]

    self.assertItemsEqual(postsubmit_orchestrator_children,
                          master_postsubmit_children)
