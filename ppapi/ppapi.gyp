# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is the "public" ppapi.gyp file, which must have dependencies on the
# redistributable portions of PPAPI only. This prevents circular dependencies
# in the .gyp files (since ppapi_internal depends on parts of Chrome).

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.

    # NaCl also uses the C and C++ PPAPI targets. We want it to be able to use
    # its own version of PPAPI when compiling into Chrome, which means we'll
    # actually have two instances of each library in the checkout (though not
    # compiled into the same binary since NaCl is a shared library).
    #
    # This value is the suffix that will be appended to the relevant projects.
    # In Chrome, it's empty so the projects have the same name. NaCl sets this
    # to "_nacl" and includes the .gypi files below, giving it different names
    # for these projects.
    'nacl_ppapi_library_suffix': '',
  },
  'target_defaults': {
    'conditions': [
      # Linux shared libraries should always be built -fPIC.
      #
      # TODO(ajwong): For internal pepper plugins, which are statically linked
      # into chrome, do we want to build w/o -fPIC?  If so, how can we express
      # that in the build system?
      ['os_posix == 1 and OS != "mac"', {
        'cflags': ['-fPIC', '-fvisibility=hidden'],

        # This is needed to make the Linux shlib build happy. Without this,
        # -fvisibility=hidden gets stripped by the exclusion in common.gypi
        # that is triggered when a shared library build is specified.
        'cflags/': [['include', '^-fvisibility=hidden$']],
      }],
    ],
  },
  'includes': [
    'ppapi_cpp.gypi',
    'ppapi_gl.gypi',
  ],
}
