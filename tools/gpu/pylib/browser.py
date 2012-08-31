# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import chrome_remote_control

def StartBrowser():
  parser = chrome_remote_control.BrowserOptions.CreateParser()
  options, _ = parser.parse_args()

  possible_browser = chrome_remote_control.FindBestPossibleBrowser(options)
  if possible_browser is None:
    raise Exception('No browsers found of the following types: %s.' %
                    options.browser_types_to_use)

  return possible_browser.Create()
