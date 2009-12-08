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
    print('usage: %s ignored inspector_html devtools_html'
          ' css_and_js_files_list' % argv[0])
    return 1

  # The first argument is ignored. We put 'webkit.gyp' in the inputs list
  # for this script, so every time the list of script gets changed, our html
  # file is rebuilt.
  inspector_html_name = argv[2]
  devtools_html_name = argv[3]
  inspector_html = open(inspector_html_name, 'r')
  devtools_html = open(devtools_html_name, 'w')

  for line in inspector_html:
    if '</head>' in line:
      devtools_html.write('\n    <!-- The following lines are added to include DevTools resources -->\n')
      for resource in argv[4:]:
        devtools_html.write(GenerateIncludeTag(resource))
      devtools_html.write('    <!-- End of auto-added files list -->\n')
    devtools_html.write(line)

  devtools_html.close()
  inspector_html.close()

  # Touch output file directory to make sure that Xcode will copy
  # modified resource files.
  if sys.platform == 'darwin':
    output_dir_name = os.path.dirname(devtools_html_name)
    os.utime(output_dir_name, None)

if __name__ == '__main__':
  sys.exit(main(sys.argv))
