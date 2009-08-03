#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script takes the samples and splits out the code and the HTML
into separate files for the unified sample browser."""

import datetime
import os.path
import re
import sys

gflags_dir = os.path.normpath(
  os.path.join(os.path.dirname(sys.argv[0]),
               '..', '..', 'third_party', 'gflags', 'python'))
sys.path.append(gflags_dir)

import gflags

year = datetime.date.today().year
copyright_header = """/*
 * Copyright %d, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
""" % year

FLAGS = gflags.FLAGS
gflags.DEFINE_string('products', None,
                     'Sets the output directory for products.')

gflags.DEFINE_string('samples', None,
                      'Sets the directory for samples..')

gflags.DEFINE_boolean('find_candidates', False,
                      'Tells the script to just output a list of '
                      'candidate samples.')

def FindCandidates():
  contents = GetFileContents(
    os.path.join(FLAGS.samples, "interactive_samples.js")).splitlines()
  start_re = re.compile("var codeArray.*")
  end_re = re.compile("];")
  keepers = []
  inside = False
  for line in contents:
    if end_re.match(line):
      inside = False
    if inside:
      keepers.append(line)
    if start_re.match(line):
      inside = True
  keepers = ["["] + keepers + ["]"]
  candidate_data = eval("\n".join(keepers),
                        {'__builtins__': None, 'true': 1, 'false': 0}, None)

  # Put them into a dict to make sure they're unique
  candidates = {}
  for category in candidate_data:
    for samples in category['samples']:
      candidates[samples['files'][0]] = 1
  candidates_array = candidates.keys()
  candidates_array.sort()
  for candidate in candidates_array:
    print candidate

def GetFileContents(filename):
  try:
    fd = open(filename, "r")
    output = fd.read()
    fd.close()
  except IOError:
    print "Unable to read file '%s'" % (filename)
    return None
  return output

def SetFileContentsIfDifferent(filename, contents):
  try:
    fd = open(filename, "r")
    old_contents = fd.read()
    fd.close()
  except IOError:
    old_contents = None
    pass

  # If nothing changed, then don't re-write anything.
  if old_contents == contents:
    return True

  try:
    fd = open(filename, "wb")
  except IOError:
    print "Unable to write file '%s'" % (filename)
    return False
  output = fd.write(contents)
  fd.close()
  return True

def SplitSample(sample_file, output_html, output_js):
  script_re = re.compile(r'(.*)<script(\s+type="text/javascript")' +
                         r'(\s+charset="utf-8")?(\s+id="o3dscript")?' +
                         r'\s*>(.*)</script>(.*)',
                         re.DOTALL)

  content = GetFileContents(sample_file)
  m = script_re.match(content)
  if not m:
    raise Exception('Script regexp failed on input file %s' % sample_file)
  (html_start, type, charset, id, script, html_end) = m.groups()
  if not type:
    raise Exception('Found a script (%s) that lacked the javascript tag!'
                % sample_file)
  if not charset:
    charset = ''

  html_content = (
      '%(html_start)s<script %(type)s%(id)s%(charset)s '
      'src="%(js_path)s"></script>%(html_end)s' %
      {
        'html_start' : html_start,
        'type' : type,
        'id' : id,
        'charset' : charset,
        'js_path' : os.path.basename(output_js),
        'html_end' : html_end
      })

  if not SetFileContentsIfDifferent(output_html, html_content):
    return False
  if not SetFileContentsIfDifferent(output_js, copyright_header + script):
    return False
  return True

def main(argv):
  try:
    files = FLAGS(argv)  # Parse flags
  except gflags.FlagsError, e:
    print '%s.\nUsage: %s [<options>]\n%s' % \
          (e, sys.argv[0], FLAGS)
    sys.exit(2)

  # Strip off argv[0] to leave a list of html files to split.
  files = files[1:]

  if FLAGS.find_candidates:
    return FindCandidates()

  for file in files:
    basename = os.path.basename(file)
    output_html = os.path.join(FLAGS.products, basename)
    output_js = os.path.splitext(output_html)[0] + '.js'
    print "Splitting sample %s into %s and %s" % (file, output_html, output_js)
    if not SplitSample(file, output_html, output_js):
      sys.exit(2)

if __name__ == "__main__":
  main(sys.argv)
