# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This entire GYP should be removed once we build the plugin only in GYP.
# NOTE:  It is possible for scons to build the plugin by running the gyp
# build with the plugin as a target so...
{
  'variables': {
    # TODO(ncbray): reenable
    # http://code.google.com/p/nativeclient/issues/detail?id=1643
    'nacl_strict_warnings': 0,
    'p2p_apis%': 1,
  },
  'includes': [
    '../../../build/common.gypi',
    '../../../../ppapi/ppapi_cpp.gypi',
  ],
}
