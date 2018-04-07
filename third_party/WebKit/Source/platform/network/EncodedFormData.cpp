/*
 * Copyright (C) 2004, 2006, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/platform/network/encoded_form_data.h"

#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

bool FormDataElement::IsSafeToSendToAnotherThread() const {
  return filename_.IsSafeToSendToAnotherThread() &&
         blob_uuid_.IsSafeToSendToAnotherThread();
}

inline EncodedFormData::EncodedFormData()
    : identifier_(0), contains_password_data_(false) {}

inline EncodedFormData::EncodedFormData(const EncodedFormData& data)
    : RefCounted<EncodedFormData>(),
      elements_(data.elements_),
      identifier_(data.identifier_),
      contains_password_data_(data.contains_password_data_) {}

EncodedFormData::~EncodedFormData() = default;

scoped_refptr<EncodedFormData> EncodedFormData::Create() {
  return base::AdoptRef(new EncodedFormData);
}

scoped_refptr<EncodedFormData> EncodedFormData::Create(const void* data,
                                                       size_t size) {
  scoped_refptr<EncodedFormData> result = Create();
  result->AppendData(data, size);
  return result;
}

scoped_refptr<EncodedFormData> EncodedFormData::Create(const CString& string) {
  scoped_refptr<EncodedFormData> result = Create();
  result->AppendData(string.data(), string.length());
  return result;
}

scoped_refptr<EncodedFormData> EncodedFormData::Create(
    const Vector<char>& vector) {
  scoped_refptr<EncodedFormData> result = Create();
  result->AppendData(vector.data(), vector.size());
  return result;
}

scoped_refptr<EncodedFormData> EncodedFormData::Copy() const {
  return base::AdoptRef(new EncodedFormData(*this));
}

scoped_refptr<EncodedFormData> EncodedFormData::DeepCopy() const {
  scoped_refptr<EncodedFormData> form_data(Create());

  form_data->identifier_ = identifier_;
  form_data->boundary_ = boundary_;
  form_data->contains_password_data_ = contains_password_data_;

  size_t n = elements_.size();
  form_data->elements_.ReserveInitialCapacity(n);
  for (size_t i = 0; i < n; ++i) {
    const FormDataElement& e = elements_[i];
    switch (e.type_) {
      case FormDataElement::kData:
        form_data->elements_.UncheckedAppend(FormDataElement(e.data_));
        break;
      case FormDataElement::kEncodedFile:
        form_data->elements_.UncheckedAppend(FormDataElement(
            e.filename_.IsolatedCopy(), e.file_start_, e.file_length_,
            e.expected_file_modification_time_));
        break;
      case FormDataElement::kEncodedBlob:
        form_data->elements_.UncheckedAppend(FormDataElement(
            e.blob_uuid_.IsolatedCopy(), e.optional_blob_data_handle_));
        break;
      case FormDataElement::kDataPipe:
        network::mojom::blink::DataPipeGetterPtr data_pipe_getter;
        (*e.data_pipe_getter_->GetPtr())
            ->Clone(mojo::MakeRequest(&data_pipe_getter));
        auto wrapped = base::MakeRefCounted<WrappedDataPipeGetter>(
            std::move(data_pipe_getter));
        form_data->elements_.UncheckedAppend(
            FormDataElement(std::move(wrapped)));
        break;
    }
  }
  return form_data;
}

void EncodedFormData::AppendData(const void* data, size_t size) {
  if (elements_.IsEmpty() || elements_.back().type_ != FormDataElement::kData)
    elements_.push_back(FormDataElement());
  FormDataElement& e = elements_.back();
  size_t old_size = e.data_.size();
  e.data_.Grow(old_size + size);
  memcpy(e.data_.data() + old_size, data, size);
}

void EncodedFormData::AppendFile(const String& filename) {
  elements_.push_back(
      FormDataElement(filename, 0, BlobData::kToEndOfFile, InvalidFileTime()));
}

void EncodedFormData::AppendFileRange(const String& filename,
                                      long long start,
                                      long long length,
                                      double expected_modification_time) {
  elements_.push_back(
      FormDataElement(filename, start, length, expected_modification_time));
}

void EncodedFormData::AppendBlob(
    const String& uuid,
    scoped_refptr<BlobDataHandle> optional_handle) {
  elements_.push_back(FormDataElement(uuid, std::move(optional_handle)));
}

void EncodedFormData::AppendDataPipe(
    scoped_refptr<WrappedDataPipeGetter> handle) {
  elements_.emplace_back(std::move(handle));
}

void EncodedFormData::Flatten(Vector<char>& data) const {
  // Concatenate all the byte arrays, but omit everything else.
  data.clear();
  size_t n = elements_.size();
  for (size_t i = 0; i < n; ++i) {
    const FormDataElement& e = elements_[i];
    if (e.type_ == FormDataElement::kData)
      data.Append(e.data_.data(), static_cast<size_t>(e.data_.size()));
  }
}

String EncodedFormData::FlattenToString() const {
  Vector<char> bytes;
  Flatten(bytes);
  return Latin1Encoding().Decode(reinterpret_cast<const char*>(bytes.data()),
                                 bytes.size());
}

unsigned long long EncodedFormData::SizeInBytes() const {
  unsigned size = 0;
  size_t n = elements_.size();
  for (size_t i = 0; i < n; ++i) {
    const FormDataElement& e = elements_[i];
    switch (e.type_) {
      case FormDataElement::kData:
        size += e.data_.size();
        break;
      case FormDataElement::kEncodedFile:
        size += e.file_length_ - e.file_start_;
        break;
      case FormDataElement::kEncodedBlob:
        if (e.optional_blob_data_handle_)
          size += e.optional_blob_data_handle_->size();
        break;
      case FormDataElement::kDataPipe:
        // We can get the size but it'd be async. Data pipe elements exist only
        // in EncodedFormData instances that were filled from the content side
        // using the WebHTTPBody interface, and generally represent blobs.
        // Since for actual kEncodedBlob elements we ignore their size as well
        // if the element was created through WebHTTPBody (which never sets
        // optional_blob_data_handle), we'll ignore the size of these elements
        // as well.
        break;
    }
  }
  return size;
}

bool EncodedFormData::IsSafeToSendToAnotherThread() const {
  if (!HasOneRef())
    return false;
  for (auto& element : elements_) {
    if (!element.IsSafeToSendToAnotherThread())
      return false;
  }
  return true;
}

}  // namespace blink
