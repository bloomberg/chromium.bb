# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module to add gyp support to cr."""

import cr


class GypPrepareOut(cr.PrepareOut):
  """A prepare action that runs gyp whenever you select an output directory."""

  ENABLED = cr.Config.From(
      GYP_GENERATORS='ninja',
      GYP_GENERATOR_FLAGS='output_dir={CR_OUT_BASE} config={CR_BUILDTYPE}',
  )

  def Prepare(self):
    if cr.context.verbose >= 1:
      print cr.context.Substitute('Invoking gyp with {GYP_GENERATOR_FLAGS}')
    cr.Host.Execute(
        '{CR_SRC}/build/gyp_chromium',
        '--depth={CR_SRC}',
        '--check'
    )
