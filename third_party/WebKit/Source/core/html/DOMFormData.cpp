/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/DOMFormData.h"

#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFormElement.h"
#include "wtf/text/TextEncoding.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

class DOMFormDataIterationSource final : public PairIterable<String, FormDataEntryValue>::IterationSource {
public:
    DOMFormDataIterationSource(DOMFormData* formData) : m_formData(formData), m_current(0) { }

    bool next(ScriptState* scriptState, String& key, FormDataEntryValue& value, ExceptionState& exceptionState) override
    {
        if (m_current >= m_formData->size())
            return false;

        const FormDataList::Entry entry = m_formData->getEntry(m_current++);
        key = entry.name();
        if (entry.isString())
            value.setUSVString(entry.string());
        else if (entry.isFile())
            value.setFile(entry.file());
        else
            ASSERT_NOT_REACHED();
        return true;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_formData);
        PairIterable<String, FormDataEntryValue>::IterationSource::trace(visitor);
    }

private:
    const Member<DOMFormData> m_formData;
    size_t m_current;
};

} // namespace

DOMFormData::DOMFormData(const WTF::TextEncoding& encoding)
    : FormDataList(encoding)
    , m_opaque(false)
{
}

DOMFormData::DOMFormData(HTMLFormElement* form)
    : FormDataList(UTF8Encoding())
    , m_opaque(false)
{
    if (!form)
        return;

    for (unsigned i = 0; i < form->associatedElements().size(); ++i) {
        FormAssociatedElement* element = form->associatedElements()[i];
        if (!toHTMLElement(element)->isDisabledFormControl())
            element->appendFormData(*this, true);
    }
}

void DOMFormData::append(const String& name, const String& value)
{
    appendData(name, value);
}

void DOMFormData::append(ExecutionContext* context, const String& name, Blob* blob, const String& filename)
{
    if (blob) {
        if (blob->isFile()) {
            if (filename.isNull())
                UseCounter::count(context, UseCounter::FormDataAppendFile);
            else
                UseCounter::count(context, UseCounter::FormDataAppendFileWithFilename);
        } else {
            if (filename.isNull())
                UseCounter::count(context, UseCounter::FormDataAppendBlob);
            else
                UseCounter::count(context, UseCounter::FormDataAppendBlobWithFilename);
        }
    } else {
        UseCounter::count(context, UseCounter::FormDataAppendNull);
    }
    appendBlob(name, blob, filename);
}

void DOMFormData::get(const String& name, FormDataEntryValue& result)
{
    if (m_opaque)
        return;
    Entry entry = getEntry(name);
    if (entry.isString())
        result.setUSVString(entry.string());
    else if (entry.isFile())
        result.setFile(entry.file());
    else
        ASSERT(entry.isNone());
}

HeapVector<FormDataEntryValue> DOMFormData::getAll(const String& name)
{
    HeapVector<FormDataEntryValue> results;

    if (m_opaque)
        return results;

    HeapVector<FormDataList::Entry> entries = FormDataList::getAll(name);
    for (const FormDataList::Entry& entry : entries) {
        ASSERT(entry.name() == name);
        FormDataEntryValue value;
        if (entry.isString())
            value.setUSVString(entry.string());
        else if (entry.isFile())
            value.setFile(entry.file());
        else
            ASSERT_NOT_REACHED();
        results.append(value);
    }
    ASSERT(results.size() == entries.size());
    return results;
}

bool DOMFormData::has(const String& name)
{
    if (m_opaque)
        return false;
    return hasEntry(name);
}

void DOMFormData::set(const String& name, const String& value)
{
    setEntry(Item(encodeAndNormalize(name), encodeAndNormalize(value)));
}

void DOMFormData::set(const String& name, Blob* blob, const String& filename)
{
    setEntry(Item(encodeAndNormalize(name), blob, filename));
}

void DOMFormData::setEntry(const Item& item)
{
    const CString keyData = item.key();
    bool found = false;
    size_t i = 0;
    while (i < m_items.size()) {
        if (m_items[i].key() != keyData) {
            ++i;
        } else if (found) {
            m_items.remove(i);
        } else {
            found = true;
            m_items[i] = item;
            ++i;
        }
    }
    if (!found)
        m_items.append(item);
    return;
}

PairIterable<String, FormDataEntryValue>::IterationSource* DOMFormData::startIteration(ScriptState*, ExceptionState&)
{
    if (m_opaque)
        return new DOMFormDataIterationSource(new DOMFormData(nullptr));

    return new DOMFormDataIterationSource(this);
}

} // namespace blink
