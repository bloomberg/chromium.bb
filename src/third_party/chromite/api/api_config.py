# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""API config object."""

from __future__ import print_function

import collections


ApiConfig = collections.namedtuple('ApiConfig', ['validate_only'])


class ApiConfigMixin(object):
  """Mixin to add an API Config factory properties.

  This is meant to be used for tests to make these configs more uniform across
  all of the tests since there's very little to change anyway.
  """

  @property
  def api_config(self):
    return ApiConfig(validate_only=False)

  @property
  def validate_only_config(self):
    return ApiConfig(validate_only=True)
