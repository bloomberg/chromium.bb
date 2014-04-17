# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates ExtensionFunctions enum in histograms.xml file with values read from
extension_function_histogram_value.h.

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

  UpdateHistogramEnum(histogram_enum_name='ExtensionFunctions',
                      source_enum_path=
                          os.path.join('..', '..', '..', 'extensions',
                                       'browser',
                                       'extension_function_histogram_value.h'),
                      start_marker='^enum HistogramValue {',
                      end_marker='^ENUM_BOUNDARY')
