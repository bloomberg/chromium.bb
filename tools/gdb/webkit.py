# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""GDB support for WebKit types.

Add this to your gdb by amending your ~/.gdbinit as follows:
  python
  import sys
  sys.path.insert(0, "/path/to/tools/gdb/")
  import webkit
"""

import gdb
import struct

def ustring_to_string(ptr, length=None):
    """Convert a pointer to UTF-16 data into a Python Unicode string.

    ptr and length are both gdb.Value objects.
    If length is unspecified, will guess at the length."""
    extra = ''
    if length is None:
        # Try to guess at the length.
        for i in xrange(0, 2048):
            if int((ptr + i).dereference()) == 0:
                length = i
                break
        if length is None:
            length = 256
            extra = u' (no trailing NUL found)'
    else:
        length = int(length)

    char_vals = [int((ptr + i).dereference()) for i in xrange(length)]
    string = struct.pack('H' * length, *char_vals).decode('utf-16', 'replace')

    return string + extra


class StringPrinter:
    "Shared code between different string-printing classes"
    def __init__(self, val):
        self.val = val

    def display_hint(self):
        return 'string'


class UCharStringPrinter(StringPrinter):
    "Print a UChar*; we must guess at the length"
    def to_string(self):
        return ustring_to_string(self.val)


class WebCoreAtomicStringPrinter(StringPrinter):
    "Print a WebCore::AtomicString"
    def to_string(self):
        return self.val['m_string']


class WebCoreStringPrinter(StringPrinter):
    "Print a WebCore::String"
    def get_length(self):
        if not self.val['m_impl']['m_ptr']:
            return 0
        return self.val['m_impl']['m_ptr']['m_length']

    def to_string(self):
        if self.get_length() == 0:
            return '(null)'

        return ustring_to_string(self.val['m_impl']['m_ptr']['m_data'],
                                 self.get_length())


def lookup_function(val):
    """Function used to load pretty printers; will be passed to GDB."""
    lookup_tag = val.type.tag
    printers = {
        "WebCore::AtomicString": WebCoreAtomicStringPrinter,
        "WebCore::String": WebCoreStringPrinter,
    }
    name = val.type.tag
    if name in printers:
        return printers[name](val)

    if val.type.code == gdb.TYPE_CODE_PTR:
        name = str(val.type.target().unqualified())
        if name == 'UChar':
            return UCharStringPrinter(val)

    return None


gdb.pretty_printers.append(lookup_function)
