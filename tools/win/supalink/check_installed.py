#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

def main():
  _winreg = None
  if sys.platform == 'win32':
    import _winreg
  elif sys.platform == 'cygwin':
    try:
      import cygwinreg as _winreg
    except ImportError:
      pass  # If not available, be safe and write 0.

  if not _winreg:
    sys.stdout.write('0')
    return 0

  try:
    val = _winreg.QueryValue(_winreg.HKEY_CURRENT_USER,
                             'Software\\Chromium\\supalink_installed')
    if os.path.exists(val):
      # Apparently gyp thinks this means there was an error?
      #sys.stderr.write('Supalink enabled.\n')
      sys.stdout.write('1')
      return 0
  except WindowsError:
    pass

  sys.stdout.write('0')
  return 0


if __name__ == '__main__':
  sys.exit(main())
