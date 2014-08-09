# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry.page import page_test
from telemetry.page import page_test
from telemetry.value import scalar


class Screenshot(page_test.PageTest):
  def __init__(self):
    super(Screenshot, self).__init__(
        action_name_to_run = 'RunPrepareForScreenshot',
        is_action_name_to_run_optional=True)

  @classmethod
  def AddCommandLineArgs(cls, parser):
    parser.add_option('--png-outdir',
                      help='Output directory for the PNG files')

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    if not args.png_outdir:
      parser.error('Please specify --png-outdir')
    cls._png_outdir = args.png_outdir

  def ValidateAndMeasurePage(self, page, tab, results):
    if not tab.screenshot_supported:
      raise page_test.TestNotSupportedOnPlatformFailure(
          'Browser does not support screenshotting')

    tab.WaitForDocumentReadyStateToBeComplete()
    screenshot = tab.Screenshot(60)

    outpath = os.path.abspath(
        os.path.join(self._png_outdir, page.file_safe_name)) + '.png'

    if os.path.exists(outpath):
      previous_mtime = os.path.getmtime(outpath)
    else:
      previous_mtime = -1

    screenshot.WritePngFile(outpath)

    saved_picture_count = 0
    if os.path.exists(outpath) and os.path.getmtime(outpath) > previous_mtime:
      saved_picture_count = 1
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'saved_picture_count', 'count',
        saved_picture_count))
