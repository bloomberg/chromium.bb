# Copyright (c) 2010 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # TODO(ncbray): reenable
    # http://code.google.com/p/nativeclient/issues/detail?id=1643
    'nacl_strict_warnings': 0,
  },
  'includes': [
    '../../../build/common.gypi',
    '../../../../ppapi/ppapi_cpp.gypi',
  ],
}
