#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import with_statement
import string
import sys


FILE_TEMPLATE = \
"""<?xml version="1.0" encoding="utf-8"?>
<!--
  This file is generated.
  Please use 'src/tools/polymer/polymer_grdp_to_txt.py' and
  'src/tools/polymer/txt_to_polymer_grdp.py' to modify it, if possible.

  'polymer_grdp_to_txt.py' converts 'polymer_resources.grdp' to a plane list of
  used Polymer components. v1.0 elements are marked with 'v1.0 ' prefix:
    ...
    core-animation/core-animation.html
    core-animation/web-animations.html
    core-collapse/core-collapse-extracted.js
    core-collapse/core-collapse.css
    core-collapse/core-collapse.html
    core-dropdown/core-dropdown-base-extracted.js
    ...
    v1.0 iron-iron-iconset/iron-iconset-extracted.js
    v1.0 iron-iron-iconset/iron-iconset.html
    ...

  'txt_to_polymer_grdp.py' converts list back to GRDP file.

  Usage:
    $ polymer_grdp_to_txt.py polymer_resources.grdp > /tmp/list.txt
    $ vim /tmp/list.txt
    $ txt_to_polymer_grdp.py /tmp/list.txt > polymer_resources.grdp
-->
<grit-part>
  <!-- Polymer 0.5 (TODO: Remove by M45 branch point) -->
%(v_0_5)s
  <structure name="IDR_POLYMER_WEB_ANIMATIONS_JS_WEB_ANIMATIONS_NEXT_LITE_MIN_JS"
             file="../../../third_party/web-animations-js/sources/web-animations-next-lite.min.js"
             type="chrome_html" />
  <structure name="IDR_POLYMER_WEB_ANIMATIONS_JS_WEB_ANIMATIONS_NEXT_LITE_MIN_JS_MAP"
             file="../../../third_party/web-animations-js/sources/web-animations-next-lite.min.js.map"
             type="chrome_html" />

  <!-- Polymer 1.0 -->
%(v_1_0)s
</grit-part>
"""


DEFINITION_TEMPLATE_0_5 = \
"""  <structure name="%s"
             file="../../../third_party/polymer/components-chromium/%s"
             type="chrome_html" />"""

DEFINITION_TEMPLATE_1_0 = \
"""  <structure name="%s"
             file="../../../third_party/polymer/v1_0/components-chromium/%s"
             type="chrome_html" />"""


def PathToGritId(path, is_1_0):
  table = string.maketrans(string.lowercase + '/.-', string.uppercase + '___')
  return 'IDR_POLYMER_' + ('1_0_' if is_1_0 else '') + path.translate(table)

def SortKey(record):
  return (record[1], PathToGritId(record[0], record[1]))


def ParseRecord(record):
  record = record.strip()
  v_1_0_prefix = 'v1.0 '
  if record.startswith(v_1_0_prefix):
    return (record[len(v_1_0_prefix):], True)
  else:
    return (record, False)

def main(argv):
  with open(argv[1]) as f:
    records = [ParseRecord(r) for r in f if not r.isspace()]
  lines = { 'v_0_5': [], 'v_1_0': [] }
  for (path, is_1_0) in sorted(set(records), key=SortKey):
    template = DEFINITION_TEMPLATE_1_0 if is_1_0 else DEFINITION_TEMPLATE_0_5
    lines['v_1_0' if is_1_0 else 'v_0_5'].append(
        template % (PathToGritId(path, is_1_0), path))
  print FILE_TEMPLATE % { 'v_0_5': '\n'.join(lines['v_0_5']),
                          'v_1_0': '\n'.join(lines['v_1_0']) }

if __name__ == '__main__':
  sys.exit(main(sys.argv))
