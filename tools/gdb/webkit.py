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


class StringPrinter(object):
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


class WebCoreQualifiedNamePrinter(StringPrinter):
    "Print a WebCore::QualifiedName"

    def __init__(self, val):
        super(WebCoreQualifiedNamePrinter, self).__init__(val)
        self.prefix_length = 0
        self.length = 0
        if self.val['m_impl']:
            self.prefix_printer = WebCoreStringPrinter(
                self.val['m_impl']['m_prefix']['m_string'])
            self.local_name_printer = WebCoreStringPrinter(
                self.val['m_impl']['m_localName']['m_string'])
            self.prefix_length = self.prefix_printer.get_length()
            if self.prefix_length > 0:
                self.length = (self.prefix_length + 1 +
                    self.local_name_printer.get_length())
            else:
                self.length = self.local_name_printer.get_length()

    def get_length(self):
        return self.length

    def to_string(self):
        if self.get_length() == 0:
            return "(null)"
        else:
            if self.prefix_length > 0:
                return (self.prefix_printer.to_string() + ":" +
                    self.local_name_printer.to_string())
            else:
                return self.local_name_printer.to_string()



def lookup_function(val):
    """Function used to load pretty printers; will be passed to GDB."""
    lookup_tag = val.type.tag
    printers = {
        "WebCore::AtomicString": WebCoreAtomicStringPrinter,
        "WebCore::String": WebCoreStringPrinter,
        "WebCore::QualifiedName": WebCoreQualifiedNamePrinter,
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



class PrintPathToRootCommand(gdb.Command):
  """Command for printing WebKit Node trees.
Usage: printpathtoroot variable_name
"""

  def __init__(self):
      super(PrintPathToRootCommand, self).__init__("printpathtoroot",
          gdb.COMMAND_SUPPORT,
          gdb.COMPLETE_NONE)

  def invoke(self, arg, from_tty):
      element_type = gdb.lookup_type('WebCore::Element')
      node_type = gdb.lookup_type('WebCore::Node')
      frame = gdb.selected_frame()
      try:
          val = gdb.Frame.read_var(frame, arg)
      except:
          print "No such variable, or invalid type"
          return

      target_type = str(val.type.target().strip_typedefs())
      if target_type == str(node_type):
          stack = []
          while val:
              stack.append([val,
                  val.cast(element_type.pointer()).dereference()['m_tagName']])
              val = val.dereference()['m_parent']

          padding = ''
          while len(stack) > 0:
              pair = stack.pop()
              print padding, pair[1], pair[0]
              padding = padding + '  '
      else:
          print 'Sorry: I don\'t know how to deal with %s yet.' % target_type

PrintPathToRootCommand()
