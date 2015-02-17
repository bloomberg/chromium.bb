# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  ######################################################################
  'includes': [
    'build/common.gypi',
  ],
  ######################################################################
  'targets' : [
    {
      # TODO(ncbray): remove this target once Chrome has been udpated.
      # https://crbug.com/456902
      'target_name': 'prep_toolchain',
      'type': 'none',
    },
  ],
}
