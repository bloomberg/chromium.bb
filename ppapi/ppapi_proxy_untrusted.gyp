# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is the "public" ppapi.gyp file, which must have dependencies on the
# redistributable portions of PPAPI only. This prevents circular dependencies
# in the .gyp files (since ppapi_internal depends on parts of Chrome).

{
  'conditions': [
    ['disable_nacl==0 and build_ppapi_ipc_proxy_untrusted==1', {
      'includes': ['ppapi_proxy_untrusted.gypi'],
    }],
  ],
  'targets': [],
}
