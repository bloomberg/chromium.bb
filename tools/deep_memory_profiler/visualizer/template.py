#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import tempfile

from string import Template


_TEMPLATE = """<!DOCTYPE html>
<meta charset="utf-8">
<link rel="stylesheet" href="../visualizer/static/index.css">
<link rel="stylesheet"
  href="../visualizer/static/third_party/jqTree/jqtree.css">

<script src="../../../third_party/flot/jquery.min.js"></script>
<script src="../../../third_party/flot/jquery.flot.min.js"></script>
<script src="../../../third_party/flot/jquery.flot.stack.min.js"></script>
<script src="../visualizer/static/third_party/jqTree/tree.jquery.js"></script>
<script src="../visualizer/static/utility.js"></script>
<script src="../visualizer/static/profiler.js"></script>
<script src="../visualizer/static/graph-view.js"></script>
<script src="../visualizer/static/dropdown-view.js"></script>
<script src="../visualizer/static/menu-view.js"></script>
<script type="text/javascript">
  $(function() {
  var data = $DATA;
  var profiler = new Profiler(data);
  var graphView = new GraphView(profiler);
  var dropdownView = new DropdownView(profiler);
  var menuView = new MenuView(profiler);

  profiler.reparse();
});
</script>

<body>
  <h2>Deep Memory Profiler Visulaizer</h2>
  <div id="graph-div"></div>
  <div id="info-div">
    <div id="category-menu"></div>
    <div id="subs-dropdown"></div>
  </div>
</body>
"""


def main(argv):
  # Read json data.
  with open(argv[1]) as data_file:
    data = data_file.read()

  # Fill in the template of index.js.
  dmprof_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  html_dir = os.path.join(dmprof_path, 'graphs')
  if not os.path.exists(html_dir):
    os.mkdir(html_dir)

  html_handle, html_path = tempfile.mkstemp('.html', 'graph', html_dir)
  html_file = os.fdopen(html_handle, 'w')
  html_file.write(Template(_TEMPLATE).safe_substitute({ 'DATA': data }))
  html_file.close()

  # Open index page in chrome automatically if permitted.
  if sys.platform.startswith('linux'):
    try:
      subprocess.call(['xdg-open', html_path])
    except OSError, exception:
      print >> sys.stderr, 'xdg-open failed:', exception
      print 'generated html file is at ' + html_path
  else:
    print 'generated html file is at ' + html_path


if __name__ == '__main__':
  sys.exit(main(sys.argv))
