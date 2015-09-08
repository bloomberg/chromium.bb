/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef FormDataList_h
#define FormDataList_h

#include "core/CoreExport.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "platform/heap/Handle.h"
#include "platform/network/FormData.h"
#include "wtf/Forward.h"
#include "wtf/text/CString.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

// TODO(tkent): Merge FormDataList into DOMFormData.
class CORE_EXPORT FormDataList : public GarbageCollected<FormDataList> {
public:
    // TODO(tkent): Merge Entry and Item.
    class Entry final {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        enum Type { None, StringType, FileType };

        Entry() : m_type(None) { }
        Entry(const String& name, const String& value) : m_type(StringType), m_name(name), m_string(value) { }
        Entry(const String& name, File* value) : m_type(FileType), m_name(name), m_file(value) { }

        bool isNone() const { return m_type == None; }
        bool isString() const { return m_type == StringType; }
        bool isFile() const { return m_type == FileType; }

        const String& name() const { ASSERT(m_type != None); return m_name; }
        const String& string() const { ASSERT(m_type == StringType); return m_string; }
        File* file() const { ASSERT(m_type == FileType); return m_file; }

        DECLARE_TRACE();

    private:
        const Type m_type;
        const String m_name;
        const String m_string;
        const Member<File> m_file;
    };

    class Item {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        Item(const CString& key) : m_key(key) { }
        Item(const CString& key, const CString& data) : m_key(key), m_data(data) { }
        Item(const CString& key, Blob* blob, const String& filename) : m_key(key), m_blob(blob), m_filename(filename) { }

        const CString& key() const { return m_key; }
        const WTF::CString& data() const { return m_data; }
        Blob* blob() const { return m_blob.get(); }
        const String& filename() const { return m_filename; }

        DECLARE_TRACE();

    private:
        CString m_key;
        WTF::CString m_data;
        Member<Blob> m_blob;
        String m_filename;
    };

    using FormDataListItems = HeapVector<FormDataList::Item>;

    void appendData(const String& key, const String& value)
    {
        appendItem(Item(encodeAndNormalize(key), encodeAndNormalize(value)));
    }
    void appendData(const String& key, const CString& value)
    {
        appendItem(Item(encodeAndNormalize(key), value));
    }
    void appendData(const String& key, int value)
    {
        appendItem(Item(encodeAndNormalize(key), encodeAndNormalize(String::number(value))));
    }
    void appendBlob(const String& key, Blob* blob, const String& filename = String())
    {
        appendItem(Item(encodeAndNormalize(key), blob, filename));
    }

    void deleteEntry(const String& key);
    Entry getEntry(const String& key) const;
    Entry getEntry(size_t index) const;
    HeapVector<Entry> getAll(const String& key) const;
    bool hasEntry(const String& key) const;
    size_t size() const { return m_items.size(); }

    const FormDataListItems& items() const { return m_items; }
    const WTF::TextEncoding& encoding() const { return m_encoding; }

    PassRefPtr<FormData> createFormData(FormData::EncodingType = FormData::FormURLEncoded);
    PassRefPtr<FormData> createMultiPartFormData();

    DECLARE_VIRTUAL_TRACE();

protected:
    explicit FormDataList(const WTF::TextEncoding&);
    CString encodeAndNormalize(const String& key) const;

    FormDataListItems m_items;

private:
    void appendKeyValuePairItemsTo(FormData*, const WTF::TextEncoding&, bool isMultiPartForm, FormData::EncodingType = FormData::FormURLEncoded);

    void appendItem(const Item&);
    Entry itemsToEntry(const Item&) const;

    WTF::TextEncoding m_encoding;
};

} // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::FormDataList::Entry);
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::FormDataList::Item);

#endif // FormDataList_h
