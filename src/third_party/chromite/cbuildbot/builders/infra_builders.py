# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing builders for infra."""

from __future__ import print_function

from chromite.cbuildbot.builders import generic_builders
from chromite.cbuildbot.stages import build_stages
from chromite.cbuildbot.stages import infra_stages


class InfraGoBuilder(generic_builders.ManifestVersionedBuilder):
  """Builder that builds infra Go binaries."""

  def RunStages(self):
    """Build and upload infra Go binaries."""
    self._RunStage(build_stages.UprevStage)
    self._RunStage(build_stages.InitSDKStage)
    self._RunStage(infra_stages.EmergeInfraGoBinariesStage)
    self._RunStage(infra_stages.PackageInfraGoBinariesStage)
    self._RunStage(infra_stages.RegisterInfraGoPackagesStage)
