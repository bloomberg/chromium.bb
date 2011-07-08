# Copyright (c) 2010 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # TODO(ncbray): reenable
    # http://code.google.com/p/nativeclient/issues/detail?id=1643
    'nacl_strict_warnings': 0,
    'p2p_apis%': 1,

    # NaCl uses the C and C++ PPAPI targets. We want it to be able to use its
    # own version of PPAPI when compiling into Chrome, which means we'll
    # actually have two instances of each library in a Chrome checkout (though
    # not compiled into the same binary since NaCl is a shared library).
    #
    # This value is the suffix that will be appended to the relevant projects.
    # In Chrome, it's empty. In NaCl we set this to "_nacl" and include the
    # shared .gypi files below, giving different names for these projects.
    'nacl_ppapi_library_suffix': '_nacl',
  },
  'includes': [
    '../../../build/common.gypi',
    '../../third_party/ppapi/ppapi_cpp.gypi',
  ],
}
