#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys

def GenerateIncludeTag(resource_path):
  (dir_name, file_name) = os.path.split(resource_path)
  if (file_name.endswith('.js')):
    return '    <script type="text/javascript" src="%s"></script>\n' % file_name
  elif (file_name.endswith('.css')):
    return '    <link rel="stylesheet" type="text/css" href="%s">\n' % file_name
  else:
    assert false


def main(argv):

  if len(argv) < 4:
    print 'usage: %s inspector_html devtools_html css_and_js_files_list' % argv[0]
    return 1

  inspector_html = open(argv[1], 'r')
  devtools_html = open(argv[2], 'w')

  for line in inspector_html:
    if '</head>' in line:
      devtools_html.write('\n    <!-- The following lines are added to include DevTools resources -->\n')
      for resource in argv[3:]:
        devtools_html.write(GenerateIncludeTag(resource))
      devtools_html.write('    <!-- End of auto-added files list -->\n')
    devtools_html.write(line)

if __name__ == '__main__':
  sys.exit(main(sys.argv))
