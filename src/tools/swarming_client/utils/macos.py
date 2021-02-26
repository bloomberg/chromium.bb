# Copyright 2020 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.
"""This is simple wrapper of objc or Carbon.File/MacOS for macos."""

import sys
import unicodedata

import six

if sys.platform == 'darwin':
  if six.PY3:
    import re

    import objc

    # Extract 43 from error like 'Mac Error -43'
    _mac_error_re = re.compile('^MAC Error -(\d+)$')

    Error = OSError

    def get_errno(e):
      m = _mac_error_re.match(e.args[0])
      if not m:
        return None
      return -int(m.groups()[0])

    def native_case(p):
      path = objc.FSRef.from_pathname(six.ensure_str(p.encode('utf-8')))
      return unicodedata.normalize('NFC', path.as_pathname())

  else:
    import Carbon.File
    import MacOS

    Error = MacOS.Error

    def get_errno(e):
      return e.args[0]

    def native_case(p):
      rel_ref, _ = Carbon.File.FSPathMakeRef(p.encode('utf-8'))
      # The OSX underlying code uses NFD but python strings are in NFC. This
      # will cause issues with os.listdir() for example. Since the dtrace log
      # *is* in NFC, normalize it here.
      return unicodedata.normalize('NFC',
                                   rel_ref.FSRefMakePath().decode('utf-8'))
