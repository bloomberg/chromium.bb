#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import shutil

import multi_page_benchmark
import skpicture_printer

class SkPicturePrinterUnitTest(multi_page_benchmark.MultiPageBenchmarkUnitTest):
  def setUp(self):
    super(SkPicturePrinterUnitTest, self).setUp()
    self._outdir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self._outdir)
    super(SkPicturePrinterUnitTest, self).tearDown()

  def CustomizeOptionsForTest(self, options):
    options.outdir = self._outdir

  def testPrintToSkPicture(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('non_scrollable_page.html')
    printer = skpicture_printer.SkPicturePrinter()
    rows = self.RunBenchmark(printer, ps)

    self.assertEqual(0, len(printer.page_failures))
    header = rows[0]
    results = rows[1:]
    self.assertEqual(['url', 'output_path'], header)
    self.assertEqual(1, len(results))

    url = results[0][0]
    self.assertTrue('non_scrollable_page.html' in url)

    outdir = results[0][1]
    self.assertTrue('non_scrollable_page_html' in outdir)
    self.assertTrue(os.path.isdir(outdir))
    self.assertEqual(['layer_0.skp'], os.listdir(outdir))

    skp_file = os.path.join(outdir, 'layer_0.skp')
    self.assertTrue(os.path.isfile(skp_file))
    self.assertTrue(os.path.getsize(skp_file) > 0)
