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

def typed_ptr(ptr):
    """Prints a pointer along with its exact type.

    By default, gdb would print just the address, which takes more
    steps to interpret.
    """
    # Returning this as a cast expression surrounded by parentheses
    # makes it easier to cut+paste inside of gdb.
    return '((%s)%s)' % (ptr.dynamic_type, ptr)

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

class ScopedRefPtrPrinter(object):
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return 'scoped_refptr' + typed_ptr(self.val['ptr_'])

class SiteInstanceImplPrinter(object):
    def __init__(self, val):
        self.val = val.cast(val.dynamic_type)

    def to_string(self):
        return 'SiteInstanceImpl@%s for %s' % (
            self.val.address, self.val['site_'])

    def children(self):
        yield ('id_', self.val['id_'])
        yield ('has_site_', self.val['has_site_'])
        if self.val['browsing_instance_']['ptr_']:
            yield ('browsing_instance_', self.val['browsing_instance_']['ptr_'])
        if self.val['process_']:
            yield ('process_', typed_ptr(self.val['process_']))
        if self.val['render_process_host_factory_']:
            yield ('render_process_host_factory_',
                   self.val['render_process_host_factory_'])

class RenderProcessHostImplPrinter(object):
    def __init__(self, val):
        self.val = val.cast(val.dynamic_type)

    def to_string(self):
        pid = ''
        child_process_launcher_ptr = (
            self.val['child_process_launcher_']['impl_']['data_']['ptr'])
        if child_process_launcher_ptr:
            context = (child_process_launcher_ptr.dereference()
                       ['context_']['ptr_'])
            if context:
                pid = ' PID %s' % str(context.dereference()
                                      ['process_']['process_'])
        return 'RenderProcessHostImpl@%s%s' % (self.val.address, pid)

    def children(self):
        yield ('id_', self.val['id_'])
        yield ('render_widget_hosts_',
               self.val['render_widget_hosts_']['data_'])
        yield ('fast_shutdown_started_', self.val['fast_shutdown_started_'])
        yield ('deleting_soon_', self.val['deleting_soon_'])
        yield ('pending_views_', self.val['pending_views_'])
        yield ('visible_widgets_', self.val['visible_widgets_'])
        yield ('backgrounded_', self.val['backgrounded_'])
        yield ('widget_helper_', self.val['widget_helper_'])
        yield ('is_initialized_', self.val['is_initialized_'])
        yield ('browser_context_', typed_ptr(self.val['browser_context_']))
        yield ('sudden_termination_allowed_',
               self.val['sudden_termination_allowed_'])
        yield ('ignore_input_events_', self.val['ignore_input_events_'])
        yield ('is_guest_', self.val['is_guest_'])


pp_set = gdb.printing.RegexpCollectionPrettyPrinter("chromium")

pp_set.add_printer('FilePath', '^FilePath$', FilePathPrinter)
pp_set.add_printer('GURL', '^GURL$', GURLPrinter)
pp_set.add_printer('content::RenderProcessHostImpl',
                   '^content::RenderProcessHostImpl$',
                   RenderProcessHostImplPrinter)
pp_set.add_printer('content::SiteInstanceImpl', '^content::SiteInstanceImpl$',
                   SiteInstanceImplPrinter)
pp_set.add_printer('scoped_refptr', '^scoped_refptr<.*>$', ScopedRefPtrPrinter)
pp_set.add_printer('string16', '^string16$', String16Printer);

gdb.printing.register_pretty_printer(gdb, pp_set)
