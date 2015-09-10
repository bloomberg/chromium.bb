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

#ifndef FormData_h
#define FormData_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/UnionTypesCore.h"
#include "core/CoreExport.h"
#include "core/html/FormDataList.h"
#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "wtf/Forward.h"

namespace WTF {
class TextEncoding;
}

namespace blink {

class Blob;
class HTMLFormElement;

// Typedef from FormData.idl:
typedef FileOrUSVString FormDataEntryValue;

class CORE_EXPORT FormData final : public FormDataList, public ScriptWrappable, public PairIterable<String, FormDataEntryValue> {
    DEFINE_WRAPPERTYPEINFO();

public:
    static FormData* create(HTMLFormElement* form = 0)
    {
        return new FormData(form);
    }

    static FormData* create(const WTF::TextEncoding& encoding)
    {
        return new FormData(encoding);
    }

    // FormData IDL interface.
    void append(const String& name, const String& value);
    void append(ExecutionContext*, const String& name, Blob*, const String& filename = String());
    void deleteEntry(const String& name);
    void get(const String& name, FormDataEntryValue& result);
    HeapVector<FormDataEntryValue> getAll(const String& name);
    bool has(const String& name);
    void set(const String& name, const String& value);
    void set(const String& name, Blob*, const String& filename = String());

    // Internal functions.

    void makeOpaque() { m_opaque = true; }
    bool opaque() const { return m_opaque; }

    // TODO(tkent): Rename appendFoo functions to |append| for consistency with
    // public function.
    void appendData(const String& key, const String& value);
    void appendData(const String& key, const CString& value);
    void appendData(const String& key, int value);
    void appendBlob(const String& key, Blob*, const String& filename = String());
    String decode(const CString& data) const;

    PassRefPtr<EncodedFormData> createFormData(EncodedFormData::EncodingType = EncodedFormData::FormURLEncoded);
    PassRefPtr<EncodedFormData> createMultiPartFormData();

private:
    explicit FormData(const WTF::TextEncoding&);
    explicit FormData(HTMLFormElement*);
    void setEntry(const Item&);
    void appendKeyValuePairItemsTo(EncodedFormData*, const WTF::TextEncoding&, bool isMultiPartForm, EncodedFormData::EncodingType = EncodedFormData::FormURLEncoded);

    IterationSource* startIteration(ScriptState*, ExceptionState&) override;
    bool m_opaque;
};

} // namespace blink

#endif // FormData_h
