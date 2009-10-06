# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'npapi',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          # Some things #include "bindings/npapi.h" and others just #include
          # "npapi.h".  Account for both flavors.
          '.',
          'bindings',
        ],
      },
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
