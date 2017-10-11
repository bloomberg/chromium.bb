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
#include "bindings/core/v8/file_or_usv_string.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/TextEncoding.h"

namespace blink {

class Blob;
class HTMLFormElement;
class ScriptState;

// Typedef from FormData.idl:
typedef FileOrUSVString FormDataEntryValue;

class CORE_EXPORT FormData final
    : public GarbageCollected<FormData>,
      public ScriptWrappable,
      public PairIterable<String, FormDataEntryValue> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static FormData* Create(HTMLFormElement* form = nullptr) {
    return new FormData(form);
  }

  static FormData* Create(const WTF::TextEncoding& encoding) {
    return new FormData(encoding);
  }
  DECLARE_TRACE();

  // FormData IDL interface.
  void append(const String& name, const String& value);
  void append(ScriptState*,
              const String& name,
              Blob*,
              const String& filename = String());
  void deleteEntry(const String& name);
  void get(const String& name, FormDataEntryValue& result);
  HeapVector<FormDataEntryValue> getAll(const String& name);
  bool has(const String& name);
  void set(const String& name, const String& value);
  void set(const String& name, Blob*, const String& filename = String());

  // Internal functions.

  const WTF::TextEncoding& Encoding() const { return encoding_; }
  class Entry;
  const HeapVector<Member<const Entry>>& Entries() const { return entries_; }
  size_t size() const { return entries_.size(); }
  void append(const String& name, int value);
  void append(const String& name, Blob*, const String& filename = String());
  String Decode(const CString& data) const;

  RefPtr<EncodedFormData> EncodeFormData(
      EncodedFormData::EncodingType = EncodedFormData::kFormURLEncoded);
  RefPtr<EncodedFormData> EncodeMultiPartFormData();

 private:
  explicit FormData(const WTF::TextEncoding&);
  explicit FormData(HTMLFormElement*);
  void SetEntry(const Entry*);
  CString EncodeAndNormalize(const String& key) const;
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;

  WTF::TextEncoding encoding_;
  // Entry pointers in m_entries never be nullptr.
  HeapVector<Member<const Entry>> entries_;
};

// Represents entry, which is a pair of a name and a value.
// https://xhr.spec.whatwg.org/#concept-formdata-entry
// Entry objects are immutable.
class FormData::Entry : public GarbageCollectedFinalized<FormData::Entry> {
 public:
  Entry(const CString& name, const CString& value)
      : name_(name), value_(value) {}
  Entry(const CString& name, Blob* blob, const String& filename)
      : name_(name), blob_(blob), filename_(filename) {}
  DECLARE_TRACE();

  bool IsString() const { return !blob_; }
  bool isFile() const { return blob_; }
  const CString& name() const { return name_; }
  const CString& Value() const { return value_; }
  Blob* GetBlob() const { return blob_.Get(); }
  File* GetFile() const;
  const String& Filename() const { return filename_; }

 private:
  const CString name_;
  const CString value_;
  const Member<Blob> blob_;
  const String filename_;
};

}  // namespace blink

#endif  // FormData_h
