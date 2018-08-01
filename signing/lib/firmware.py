# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromeOS firmware Signers"""

from __future__ import print_function

from chromite.signing.lib.signer import FutilitySigner


class ECSigner(FutilitySigner):
  """Sign EC bin file."""

  _required_keys_private = ('ec',)

  def GetFutilityArgs(self, keyset, input_name, output_name):
    return ['sign',
            '--type', 'rwsig',
            '--prikey', keyset.keys['ec'].private,
            input_name,
            output_name]
