# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'irt_test_nexe': 'irt_core',
    'irt_test_dep': 'irt.gyp:irt_core_nexe',
  },
  'includes': [
    '../../../build/common.gypi',
    'check_tls.gypi',
  ],
}
