# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""GDB support for Chrome types.

Add this to your gdb by amending your ~/.gdbinit as follows:
  python
  import sys
  sys.path.insert(0, "/path/to/tools/gdb/")
  import gdb_chrome

This module relies on the WebKit gdb module already existing in
your Python path.
"""

import gdb
import webkit

class String16Printer(webkit.StringPrinter):
    def to_string(self):
        return webkit.ustring_to_string(self.val['_M_dataplus']['_M_p'])

class GURLPrinter(webkit.StringPrinter):
    def to_string(self):
        return self.val['spec_']

class FilePathPrinter(object):
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return self.val['path_']['_M_dataplus']['_M_p']


def lookup_function(val):
    type_to_printer = {
        'string16': String16Printer,
        'GURL': GURLPrinter,
        'FilePath': FilePathPrinter,
    }

    printer = type_to_printer.get(str(val.type), None)
    if printer:
        return printer(val)
    return None

gdb.pretty_printers.append(lookup_function)
