#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import polymer
import os
import shutil
import tempfile
import unittest


_HERE_DIR = os.path.dirname(__file__)


class HtmlToJsTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None
    self._tmp_dirs = []
    self._tmp_src_dir = None

  def tearDown(self):
    for tmp_dir in self._tmp_dirs:
      shutil.rmtree(tmp_dir)

  def _write_file_to_src_dir(self, file_path, file_contents):
    if not self._tmp_src_dir:
      self._tmp_src_dir = self._create_tmp_dir()
    file_path_normalized = os.path.normpath(os.path.join(self._tmp_src_dir,
                                                         file_path))
    file_dir = os.path.dirname(file_path_normalized)
    if not os.path.exists(file_dir):
      os.makedirs(file_dir)
    with open(file_path_normalized, 'w') as tmp_file:
      tmp_file.write(file_contents)

  def _create_tmp_dir(self):
    tmp_dir = tempfile.mkdtemp(dir=_HERE_DIR)
    self._tmp_dirs.append(tmp_dir)
    return tmp_dir

  def _read_out_file(self, file_name):
    assert self._out_folder
    return open(os.path.join(self._out_folder, file_name), 'r').read()

  def _run_html_to_js(self, js_file, html_file, html_type):
    assert not self._out_folder
    self._out_folder = self._create_tmp_dir()
    polymer.main([
      '--in_folder', self._tmp_src_dir,
      '--out_folder', self._out_folder,
      '--js_file',  js_file,
      '--html_file',  html_file,
      '--html_type',  html_type,
    ])

  def _run_test_(self, html_type, src_html, src_js, expected_js):
    self._write_file_to_src_dir('foo.html', src_html)
    self._write_file_to_src_dir('foo.js', src_js)
    self._run_html_to_js('foo.js', 'foo.html', html_type)
    actual_js = self._read_out_file('foo.js')
    self.assertEquals(expected_js, actual_js)

  # Test case where HTML is extracted from a Polymer2 <dom-module>.
  def testSuccess_DomModule(self):
    self._run_test_('dom-module', '''
<link rel="import" href="../../foo/bar.html">
<link rel="import" href="chrome://resources/foo/bar.html">

<dom-module id="cr-checkbox">
  <template>
    <style>
      div {
        font-size: 2rem;
      }
    </style>
    <div>Hello world</div>
  </template>
  <script src="foo.js"></script>
</dom-module>
''', '''
Polymer({
  is: 'cr-foo',

  _template: html`
{__html_template__}
  `,
});
''', '''
Polymer({
  is: 'cr-foo',

  _template: html`
    <style>
      div {
        font-size: 2rem;
      }
    </style>
    <div>Hello world</div>

  `,
});
''')

  # Test case where HTML is extracted from a Polymer2 <custom-style>.
  def testSuccess_CustomStyle(self):
    self._run_test_('custom-style', '''
<link rel="import" href="../../foo/bar.html">
<link rel="import" href="chrome://resources/foo/bar.html">

<custom-style>
  <style>
    html {
      --foo-bar: 2rem;
    }
  </style>
</custom-style>
''', '''
$_documentContainer.innerHTML = `
{__html_template__}
  `;
''', '''
$_documentContainer.innerHTML = `
<custom-style>
  <style>
    html {
      --foo-bar: 2rem;
    }
  </style>
</custom-style>

  `;
''')

  # Test case where the provided HTML is already in the form needed by Polymer3.
  def testSuccess_V3Ready(self):
    self._run_test_('v3-ready', '''<style>
  div {
    font-size: 2rem;
  }
</style>
<div>Hello world</div>
''', '''
Polymer({
  is: 'cr-foo',

  _template: html`
{__html_template__}
  `,
});
''', '''
Polymer({
  is: 'cr-foo',

  _template: html`
<style>
  div {
    font-size: 2rem;
  }
</style>
<div>Hello world</div>

  `,
});
''')

if __name__ == '__main__':
  unittest.main()
