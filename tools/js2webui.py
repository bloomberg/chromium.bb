#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used by chrome_tests.gypi's js2webui action to maintain the
argument lists and to generate inlinable tests for webui testing.

Usage:
  python tools/js2webui.py -p product_dir path/to/javascript2webui.js
  python tools/js2webui.py -t # print test_harnesses
  python tools/js2webui.py -i # print inputs
  python tools/js2webui.py -o # print outputs
"""

try:
  import json
except ImportError:
  import simplejson as json
import optparse
import os
import subprocess
import sys

# Please adjust the following to edit or add new javascript webui tests.
rules = [
    [
        'WebUIBrowserTestPass',
        'test/data/webui/sample_pass.js',
        'browser/ui/webui/web_ui_browsertest-inl.h',
    ],
]

def main ():
  """Run the program"""
  # For options -t, -i, & -o, we print the "column" of the |rules|.  We keep a
  # set of indices to print in |print_rule_indices| and print them in sorted
  # order if non-empty.
  parser = optparse.OptionParser()
  parser.set_usage(
      "%prog [-v][-n] --product_dir PRODUCT_DIR -or- "
      "%prog [-v][-n] (-i|-t|-o)")
  parser.add_option('-v', '--verbose', action='store_true')
  parser.add_option('-n', '--impotent', action='store_true',
                    help="don't execute; just print (as if verbose)")
  parser.add_option(
      '-p', '--product_dir',
      help='for gyp to set the <(PRODUCT_DIR) for running v8_shell')
  parser.add_option('-t', '--test_fixture', action='store_const', const=0,
                    dest='print_rule_index', help='print test_fixtures')
  parser.add_option('-i', '--in', action='store_const', const=1,
                    dest='print_rule_index', help='print inputs')
  parser.add_option('-o', '--out', action='store_const', const=2,
                    dest='print_rule_index', help='print outputs')
  (opts, args) = parser.parse_args()

  if (opts.print_rule_index != None):
    for rule in rules:
      print rule[opts.print_rule_index]
  else:
    if not opts.product_dir:
      parser.error("--product_dir option is required")
    v8_shell = os.path.join(opts.product_dir, 'v8_shell')
    for (test_fixture, input_js, output_cc) in rules:
      cmd = [v8_shell, '--print_json_ast', input_js]
      if opts.verbose or opts.impotent:
        print cmd
      proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
      ast = json.load(proc.stdout)
      if not opts.impotent:
        sys.stdout = open(output_cc, 'w')
      print '// GENERATED FILE'
      print '// ', ' '.join(sys.argv)
      print '// PLEASE DO NOT HAND EDIT!\n'
      for declaration in ast[2:]:
        try:
          function_literal = declaration[3]
          function_name = function_literal[1]['name']
          print 'IN_PROC_BROWSER_TEST_F(%s, %s) {' % (
              test_fixture, function_name)
          print '  AddLibrary(FilePath(FILE_PATH_LITERAL("%s")));' % (
              os.path.basename(input_js))
          print '  ASSERT_TRUE(RunJavascriptTest("%s"));' % (function_name)
          print '}\n'
        except IndexError:
          if opts.verbose or opts.impotent:
            print "skipping declaration ", declaration

if __name__ == '__main__':
 sys.exit(main())
