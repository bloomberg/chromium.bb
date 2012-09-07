# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A library for chrome-based tests.

"""
from browser_finder import *
from browser_options import *
from browser import *
from tab import *
from util import *

def CreateBrowser(type):
  """Shorthand way to create a browser of a given type

  However, note that the preferred way to create a browser is:
     options = BrowserOptions()
     _, leftover_args, = options.CreateParser().parse_args()
     browser_to_create = FindBrowser(options)
     return browser_to_create.Create()

  as it creates more opportunities for customization and
  error handling."""
  browser_to_create = FindBrowser(BrowserOptions(type))
  if not browser_to_create:
    raise Exception("No browser of type %s found" % type)
  return browser_to_create.Create()
