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

#include "core/html/FormData.h"

#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFormElement.h"
#include "platform/bindings/ScriptState.h"
#include "platform/network/FormDataEncoder.h"
#include "platform/text/LineEnding.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

namespace {

class FormDataIterationSource final
    : public PairIterable<String, FormDataEntryValue>::IterationSource {
 public:
  FormDataIterationSource(FormData* form_data)
      : form_data_(form_data), current_(0) {}

  bool Next(ScriptState* script_state,
            String& name,
            FormDataEntryValue& value,
            ExceptionState& exception_state) override {
    if (current_ >= form_data_->size())
      return false;

    const FormData::Entry& entry = *form_data_->Entries()[current_++];
    name = form_data_->Decode(entry.name());
    if (entry.IsString()) {
      value.setUSVString(form_data_->Decode(entry.Value()));
    } else {
      DCHECK(entry.isFile());
      value.setFile(entry.GetFile());
    }
    return true;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(form_data_);
    PairIterable<String, FormDataEntryValue>::IterationSource::Trace(visitor);
  }

 private:
  const Member<FormData> form_data_;
  size_t current_;
};

}  // namespace

FormData::FormData(const WTF::TextEncoding& encoding) : encoding_(encoding) {}

FormData::FormData(HTMLFormElement* form) : encoding_(UTF8Encoding()) {
  if (!form)
    return;

  for (unsigned i = 0; i < form->ListedElements().size(); ++i) {
    ListedElement* element = form->ListedElements()[i];
    if (!ToHTMLElement(element)->IsDisabledFormControl())
      element->AppendToFormData(*this);
  }
}

DEFINE_TRACE(FormData) {
  visitor->Trace(entries_);
}

void FormData::append(const String& name, const String& value) {
  entries_.push_back(
      new Entry(EncodeAndNormalize(name), EncodeAndNormalize(value)));
}

void FormData::append(ScriptState* script_state,
                      const String& name,
                      Blob* blob,
                      const String& filename) {
  if (!blob) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kFormDataAppendNull);
  }
  append(name, blob, filename);
}

void FormData::deleteEntry(const String& name) {
  const CString encoded_name = EncodeAndNormalize(name);
  size_t i = 0;
  while (i < entries_.size()) {
    if (entries_[i]->name() == encoded_name) {
      entries_.erase(i);
    } else {
      ++i;
    }
  }
}

void FormData::get(const String& name, FormDataEntryValue& result) {
  const CString encoded_name = EncodeAndNormalize(name);
  for (const auto& entry : Entries()) {
    if (entry->name() == encoded_name) {
      if (entry->IsString()) {
        result.setUSVString(Decode(entry->Value()));
      } else {
        DCHECK(entry->isFile());
        result.setFile(entry->GetFile());
      }
      return;
    }
  }
}

HeapVector<FormDataEntryValue> FormData::getAll(const String& name) {
  HeapVector<FormDataEntryValue> results;

  const CString encoded_name = EncodeAndNormalize(name);
  for (const auto& entry : Entries()) {
    if (entry->name() != encoded_name)
      continue;
    FormDataEntryValue value;
    if (entry->IsString()) {
      value.setUSVString(Decode(entry->Value()));
    } else {
      DCHECK(entry->isFile());
      value.setFile(entry->GetFile());
    }
    results.push_back(value);
  }
  return results;
}

bool FormData::has(const String& name) {
  const CString encoded_name = EncodeAndNormalize(name);
  for (const auto& entry : Entries()) {
    if (entry->name() == encoded_name)
      return true;
  }
  return false;
}

void FormData::set(const String& name, const String& value) {
  SetEntry(new Entry(EncodeAndNormalize(name), EncodeAndNormalize(value)));
}

void FormData::set(const String& name, Blob* blob, const String& filename) {
  SetEntry(new Entry(EncodeAndNormalize(name), blob, filename));
}

void FormData::SetEntry(const Entry* entry) {
  DCHECK(entry);
  const CString encoded_name = entry->name();
  bool found = false;
  size_t i = 0;
  while (i < entries_.size()) {
    if (entries_[i]->name() != encoded_name) {
      ++i;
    } else if (found) {
      entries_.erase(i);
    } else {
      found = true;
      entries_[i] = entry;
      ++i;
    }
  }
  if (!found)
    entries_.push_back(entry);
}

void FormData::append(const String& name, int value) {
  append(name, String::Number(value));
}

void FormData::append(const String& name, Blob* blob, const String& filename) {
  entries_.push_back(new Entry(EncodeAndNormalize(name), blob, filename));
}

CString FormData::EncodeAndNormalize(const String& string) const {
  CString encoded_string =
      encoding_.Encode(string, WTF::kEntitiesForUnencodables);
  return NormalizeLineEndingsToCRLF(encoded_string);
}

String FormData::Decode(const CString& data) const {
  return Encoding().Decode(data.data(), data.length());
}

RefPtr<EncodedFormData> FormData::EncodeFormData(
    EncodedFormData::EncodingType encoding_type) {
  RefPtr<EncodedFormData> form_data = EncodedFormData::Create();
  Vector<char> encoded_data;
  for (const auto& entry : Entries())
    FormDataEncoder::AddKeyValuePairAsFormData(
        encoded_data, entry->name(),
        entry->isFile() ? EncodeAndNormalize(entry->GetFile()->name())
                        : entry->Value(),
        encoding_type);
  form_data->AppendData(encoded_data.data(), encoded_data.size());
  return form_data;
}

RefPtr<EncodedFormData> FormData::EncodeMultiPartFormData() {
  RefPtr<EncodedFormData> form_data = EncodedFormData::Create();
  form_data->SetBoundary(FormDataEncoder::GenerateUniqueBoundaryString());
  Vector<char> encoded_data;
  for (const auto& entry : Entries()) {
    Vector<char> header;
    FormDataEncoder::BeginMultiPartHeader(header, form_data->Boundary().data(),
                                          entry->name());

    // If the current type is blob, then we also need to include the
    // filename.
    if (entry->GetBlob()) {
      String name;
      if (entry->GetBlob()->IsFile()) {
        File* file = ToFile(entry->GetBlob());
        // For file blob, use the filename (or relative path if it is
        // present) as the name.
        name = file->webkitRelativePath().IsEmpty()
                   ? file->name()
                   : file->webkitRelativePath();

        // If a filename is passed in FormData.append(), use it instead
        // of the file blob's name.
        if (!entry->Filename().IsNull())
          name = entry->Filename();
      } else {
        // For non-file blob, use the filename if it is passed in
        // FormData.append().
        if (!entry->Filename().IsNull())
          name = entry->Filename();
        else
          name = "blob";
      }

      // We have to include the filename=".." part in the header, even if
      // the filename is empty.
      FormDataEncoder::AddFilenameToMultiPartHeader(header, Encoding(), name);

      // Add the content type if available, or "application/octet-stream"
      // otherwise (RFC 1867).
      String content_type;
      if (entry->GetBlob()->type().IsEmpty())
        content_type = "application/octet-stream";
      else
        content_type = entry->GetBlob()->type();
      FormDataEncoder::AddContentTypeToMultiPartHeader(header,
                                                       content_type.Latin1());
    }

    FormDataEncoder::FinishMultiPartHeader(header);

    // Append body
    form_data->AppendData(header.data(), header.size());
    if (entry->GetBlob()) {
      if (entry->GetBlob()->HasBackingFile()) {
        File* file = ToFile(entry->GetBlob());
        // Do not add the file if the path is empty.
        if (!file->GetPath().IsEmpty())
          form_data->AppendFile(file->GetPath());
        if (!file->FileSystemURL().IsEmpty())
          form_data->AppendFileSystemURL(file->FileSystemURL());
      } else {
        form_data->AppendBlob(entry->GetBlob()->Uuid(),
                              entry->GetBlob()->GetBlobDataHandle());
      }
    } else {
      form_data->AppendData(entry->Value().data(), entry->Value().length());
    }
    form_data->AppendData("\r\n", 2);
  }
  FormDataEncoder::AddBoundaryToMultiPartHeader(
      encoded_data, form_data->Boundary().data(), true);
  form_data->AppendData(encoded_data.data(), encoded_data.size());
  return form_data;
}

PairIterable<String, FormDataEntryValue>::IterationSource*
FormData::StartIteration(ScriptState*, ExceptionState&) {
  return new FormDataIterationSource(this);
}

// ----------------------------------------------------------------

DEFINE_TRACE(FormData::Entry) {
  visitor->Trace(blob_);
}

File* FormData::Entry::GetFile() const {
  DCHECK(GetBlob());
  // The spec uses the passed filename when inserting entries into the list.
  // Here, we apply the filename (if present) as an override when extracting
  // entries.
  // FIXME: Consider applying the name during insertion.

  if (GetBlob()->IsFile()) {
    File* file = ToFile(GetBlob());
    if (Filename().IsNull())
      return file;
    return file->Clone(Filename());
  }

  String filename = filename_;
  if (filename.IsNull())
    filename = "blob";
  return File::Create(filename, CurrentTimeMS(),
                      GetBlob()->GetBlobDataHandle());
}

}  // namespace blink
