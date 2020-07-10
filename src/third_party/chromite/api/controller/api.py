# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""API information controller."""

from __future__ import print_function

from chromite.api import faux
from chromite.api import router as router_lib
from chromite.api import validate

# API version number.
# The major version MUST be updated on breaking changes.
VERSION_MAJOR = 1
# The minor and bug versions are not currently utilized, but put in place
# to simplify future requirements.
VERSION_MINOR = 0
VERSION_BUG = 0


@faux.all_empty
@validate.validation_complete
def GetMethods(_input_proto, output_proto, _config):
  """List all of the registered methods."""
  router = router_lib.GetRouter()
  for method in router.ListMethods():
    output_proto.methods.add().method = method


@validate.validation_complete
def GetVersion(_input_proto, output_proto, _config):
  """Get the Build API major version number."""
  output_proto.version.major = VERSION_MAJOR
  output_proto.version.minor = VERSION_MINOR
  output_proto.version.bug = VERSION_BUG
