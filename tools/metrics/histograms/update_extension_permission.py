# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates ExtensionPermission2 enum in histograms.xml file with values read
from permission_message.h.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

import os
import sys

from update_histogram_enum import UpdateHistogramEnum

if __name__ == '__main__':
  if len(sys.argv) > 1:
    print >>sys.stderr, 'No arguments expected!'
    sys.stderr.write(__doc__)
    sys.exit(1)

  UpdateHistogramEnum(histogram_enum_name='ExtensionPermission2',
                      source_enum_path=os.path.join('..', '..', '..',
                                                    'extensions', 'common',
                                                    'permissions',
                                                    'permission_message.h'),
                      start_marker='^enum ID {',
                      end_marker='^kEnumBoundary')
