# Copyright (C) 2012 Apple. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
    LLDB Support for WebKit Types

    Add the following to your .lldbinit file to add WebKit Type summaries in LLDB and Xcode:

    command script import {Path to WebKit Root}/Tools/lldb/lldb_webkit.py

"""

import lldb


def __lldb_init_module(debugger, dict):
    debugger.HandleCommand('type summary add --expand -F lldb_webkit.WTFString_SummaryProvider WTF::String')
    debugger.HandleCommand('type summary add --expand -F lldb_webkit.WTFStringImpl_SummaryProvider WTF::StringImpl')
    debugger.HandleCommand('type summary add --expand -F lldb_webkit.WTFAtomicString_SummaryProvider WTF::AtomicString')
    debugger.HandleCommand('type summary add --expand -F lldb_webkit.WTFVector_SummaryProvider -x "WTF::Vector<.+>$"')
    debugger.HandleCommand('type summary add --expand -F lldb_webkit.WTFHashTable_SummaryProvider -x "WTF::HashTable<.+>$"')
    debugger.HandleCommand('type synthetic add -x "WTF::Vector<.+>$" --python-class lldb_webkit.WTFVectorProvider')
    debugger.HandleCommand('type synthetic add -x "WTF::HashTable<.+>$" --python-class lldb_webkit.WTFHashTableProvider')
    debugger.HandleCommand('type summary add -F lldb_webkit.WebCoreLayoutUnit_SummaryProvider blink::LayoutUnit')
    debugger.HandleCommand('type summary add -F lldb_webkit.WebCoreLayoutSize_SummaryProvider blink::LayoutSize')
    debugger.HandleCommand('type summary add -F lldb_webkit.WebCoreLayoutPoint_SummaryProvider blink::LayoutPoint')


def WTFString_SummaryProvider(valobj, dict):
    provider = WTFStringProvider(valobj, dict)
    return "{ length = %d, contents = '%s' }" % (provider.get_length(), provider.to_string())


def WTFStringImpl_SummaryProvider(valobj, dict):
    provider = WTFStringImplProvider(valobj, dict)
    return "{ length = %d, is8bit = %d, contents = '%s' }" % (provider.get_length(), provider.is_8bit(), provider.to_string())


def WTFAtomicString_SummaryProvider(valobj, dict):
    return WTFString_SummaryProvider(valobj.GetChildMemberWithName('string_'), dict)


def WTFVector_SummaryProvider(valobj, dict):
    provider = WTFVectorProvider(valobj, dict)
    return "{ size = %d, capacity = %d }" % (provider.size, provider.capacity)


def WTFHashTable_SummaryProvider(valobj, dict):
    provider = WTFHashTableProvider(valobj, dict)
    return "{ tableSize = %d, keyCount = %d }" % (provider.tableSize(), provider.keyCount())


def WebCoreLayoutUnit_SummaryProvider(valobj, dict):
    provider = WebCoreLayoutUnitProvider(valobj, dict)
    return "{ %s }" % provider.to_string()


def WebCoreLayoutSize_SummaryProvider(valobj, dict):
    provider = WebCoreLayoutSizeProvider(valobj, dict)
    return "{ width = %s, height = %s }" % (provider.get_width(), provider.get_height())


def WebCoreLayoutPoint_SummaryProvider(valobj, dict):
    provider = WebCoreLayoutPointProvider(valobj, dict)
    return "{ x = %s, y = %s }" % (provider.get_x(), provider.get_y())

# FIXME: Provide support for the following types:
# def WTFCString_SummaryProvider(valobj, dict):
# def WebCoreKURLGooglePrivate_SummaryProvider(valobj, dict):
# def WebCoreQualifiedName_SummaryProvider(valobj, dict):
# def JSCIdentifier_SummaryProvider(valobj, dict):
# def JSCJSString_SummaryProvider(valobj, dict):


def guess_string_length(valobj, error):
    if not valobj.GetValue():
        return 0

    for i in xrange(0, 2048):
        if valobj.GetPointeeData(i, 1).GetUnsignedInt16(error, 0) == 0:
            return i

    return 256


def ustring_to_string(valobj, error, length=None):
    if length is None:
        length = guess_string_length(valobj, error)
    else:
        length = int(length)

    out_string = u""
    for i in xrange(0, length):
        char_value = valobj.GetPointeeData(i, 1).GetUnsignedInt16(error, 0)
        out_string = out_string + unichr(char_value)

    return out_string.encode('utf-8')


def lstring_to_string(valobj, error, length=None):
    if length is None:
        length = guess_string_length(valobj, error)
    else:
        length = int(length)

    out_string = u""
    for i in xrange(0, length):
        char_value = valobj.GetPointeeData(i, 1).GetUnsignedInt8(error, 0)
        out_string = out_string + unichr(char_value)

    return out_string.encode('utf-8')


class WTFStringImplProvider:
    def __init__(self, valobj, dict):
        self.valobj = valobj

    def get_length(self):
        return self.valobj.GetChildMemberWithName('length_').GetValueAsUnsigned(0)

    def get_data8(self):
        # FIXME: This should be the equivalent of reinterpret_cast<LChar*>(self.valobj + 1)
        return self.valobj.GetChildAtIndex(2).GetChildMemberWithName('m_data8')

    def get_data16(self):
        # FIXME: This should be the equivalent of reinterpret_cast<UChar*>(self.valobj + 1)
        return self.valobj.GetChildAtIndex(2).GetChildMemberWithName('m_data16')

    def to_string(self):
        error = lldb.SBError()
        if self.is_8bit():
            return lstring_to_string(self.get_data8(), error, self.get_length())
        return ustring_to_string(self.get_data16(), error, self.get_length())

    def is_8bit(self):
        return self.valobj.GetChildMemberWithName('is8_bit_')


class WTFStringProvider:
    def __init__(self, valobj, dict):
        self.valobj = valobj

    def stringimpl(self):
        impl_ptr = self.valobj.GetChildMemberWithName('impl_').GetChildMemberWithName('ptr_')
        return WTFStringImplProvider(impl_ptr, dict)

    def get_length(self):
        impl = self.stringimpl()
        if not impl:
            return 0
        return impl.get_length()

    def to_string(self):
        impl = self.stringimpl()
        if not impl:
            return u""
        return impl.to_string()


class WebCoreLayoutUnitProvider:
    "Print a blink::LayoutUnit"
    def __init__(self, valobj, dict):
        self.valobj = valobj

    def to_string(self):
        return "%gpx" % (self.valobj.GetChildMemberWithName('value_').GetValueAsUnsigned(0) / 64.0)


class WebCoreLayoutSizeProvider:
    "Print a blink::LayoutSize"
    def __init__(self, valobj, dict):
        self.valobj = valobj

    def get_width(self):
        return WebCoreLayoutUnitProvider(self.valobj.GetChildMemberWithName('width_'), dict).to_string()

    def get_height(self):
        return WebCoreLayoutUnitProvider(self.valobj.GetChildMemberWithName('height_'), dict).to_string()


class WebCoreLayoutPointProvider:
    "Print a blink::LayoutPoint"
    def __init__(self, valobj, dict):
        self.valobj = valobj

    def get_x(self):
        return WebCoreLayoutUnitProvider(self.valobj.GetChildMemberWithName('x_'), dict).to_string()

    def get_y(self):
        return WebCoreLayoutUnitProvider(self.valobj.GetChildMemberWithName('y_'), dict).to_string()


class WTFVectorProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def num_children(self):
        return self.size + 3

    def get_child_index(self, name):
        if name == "size_":
            return self.size
        elif name == "capacity_":
            return self.size + 1
        elif name == "buffer_":
            return self.size + 2
        else:
            return int(name.lstrip('[').rstrip(']'))

    def get_child_at_index(self, index):
        if index == self.size:
            return self.valobj.GetChildMemberWithName("size_")
        elif index == self.size + 1:
            return self.valobj.GetChildMemberWithName("capacity_")
        elif index == self.size + 2:
            return self.buffer
        elif index < self.size:
            offset = index * self.data_size
            child = self.buffer.CreateChildAtOffset('[' + str(index) + ']', offset, self.data_type)
            return child
        else:
            return None

    def update(self):
        self.buffer = self.valobj.GetChildMemberWithName('buffer_')
        self.size = self.valobj.GetChildMemberWithName('size_').GetValueAsUnsigned(0)
        self.capacity = self.buffer.GetChildMemberWithName('capacity_').GetValueAsUnsigned(0)
        self.data_type = self.buffer.GetType().GetPointeeType()
        self.data_size = self.data_type.GetByteSize()

    def has_children(self):
        return True


class WTFHashTableProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def num_children(self):
        return self.tableSize() + 5

    def get_child_index(self, name):
        if name == "table_":
            return self.tableSize()
        elif name == "table_size_":
            return self.tableSize() + 1
        elif name == "table_size_mask_":
            return self.tableSize() + 2
        elif name == "key_count_":
            return self.tableSize() + 3
        elif name == "deleted_count_":
            return self.tableSize() + 4
        else:
            return int(name.lstrip('[').rstrip(']'))

    def get_child_at_index(self, index):
        if index == self.tableSize():
            return self.valobj.GetChildMemberWithName('table_')
        elif index == self.tableSize() + 1:
            return self.valobj.GetChildMemberWithName('table_size_')
        elif index == self.tableSize() + 2:
            return self.valobj.GetChildMemberWithName('table_size_mask_')
        elif index == self.tableSize() + 3:
            return self.valobj.GetChildMemberWithName('key_count_')
        elif index == self.tableSize() + 4:
            return self.valobj.GetChildMemberWithName('deleted_count_')
        elif index < self.tableSize():
            table = self.valobj.GetChildMemberWithName('table_')
            return table.CreateChildAtOffset('[' + str(index) + ']', index * self.data_size, self.data_type)
        else:
            return None

    def tableSize(self):
        return self.valobj.GetChildMemberWithName('table_size_').GetValueAsUnsigned(0)

    def keyCount(self):
        return self.valobj.GetChildMemberWithName('key_count_').GetValueAsUnsigned(0)

    def update(self):
        self.data_type = self.valobj.GetType().GetTemplateArgumentType(0)
        self.data_size = self.data_type.GetByteSize()

    def has_children(self):
        return True
