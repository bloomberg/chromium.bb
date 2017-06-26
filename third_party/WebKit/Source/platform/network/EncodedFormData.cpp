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

#include "platform/network/EncodedFormData.h"

#include "platform/FileMetadata.h"
#include "platform/network/FormDataEncoder.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/TextEncoding.h"

namespace blink {

bool FormDataElement::IsSafeToSendToAnotherThread() const {
  return filename_.IsSafeToSendToAnotherThread() &&
         blob_uuid_.IsSafeToSendToAnotherThread() &&
         file_system_url_.IsSafeToSendToAnotherThread();
}

inline EncodedFormData::EncodedFormData()
    : identifier_(0), contains_password_data_(false) {}

inline EncodedFormData::EncodedFormData(const EncodedFormData& data)
    : RefCounted<EncodedFormData>(),
      elements_(data.elements_),
      identifier_(data.identifier_),
      contains_password_data_(data.contains_password_data_) {}

EncodedFormData::~EncodedFormData() {}

PassRefPtr<EncodedFormData> EncodedFormData::Create() {
  return AdoptRef(new EncodedFormData);
}

PassRefPtr<EncodedFormData> EncodedFormData::Create(const void* data,
                                                    size_t size) {
  RefPtr<EncodedFormData> result = Create();
  result->AppendData(data, size);
  return result;
}

PassRefPtr<EncodedFormData> EncodedFormData::Create(const CString& string) {
  RefPtr<EncodedFormData> result = Create();
  result->AppendData(string.data(), string.length());
  return result;
}

PassRefPtr<EncodedFormData> EncodedFormData::Create(
    const Vector<char>& vector) {
  RefPtr<EncodedFormData> result = Create();
  result->AppendData(vector.data(), vector.size());
  return result;
}

PassRefPtr<EncodedFormData> EncodedFormData::Copy() const {
  return AdoptRef(new EncodedFormData(*this));
}

PassRefPtr<EncodedFormData> EncodedFormData::DeepCopy() const {
  RefPtr<EncodedFormData> form_data(Create());

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
      case FormDataElement::kEncodedFileSystemURL:
        form_data->elements_.UncheckedAppend(FormDataElement(
            e.file_system_url_.Copy(), e.file_start_, e.file_length_,
            e.expected_file_modification_time_));
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
  elements_.push_back(FormDataElement(filename, 0, BlobDataItem::kToEndOfFile,
                                      InvalidFileTime()));
}

void EncodedFormData::AppendFileRange(const String& filename,
                                      long long start,
                                      long long length,
                                      double expected_modification_time) {
  elements_.push_back(
      FormDataElement(filename, start, length, expected_modification_time));
}

void EncodedFormData::AppendBlob(const String& uuid,
                                 PassRefPtr<BlobDataHandle> optional_handle) {
  elements_.push_back(FormDataElement(uuid, std::move(optional_handle)));
}

void EncodedFormData::AppendFileSystemURL(const KURL& url) {
  elements_.push_back(
      FormDataElement(url, 0, BlobDataItem::kToEndOfFile, InvalidFileTime()));
}

void EncodedFormData::AppendFileSystemURLRange(
    const KURL& url,
    long long start,
    long long length,
    double expected_modification_time) {
  elements_.push_back(
      FormDataElement(url, start, length, expected_modification_time));
}

void EncodedFormData::Flatten(Vector<char>& data) const {
  // Concatenate all the byte arrays, but omit any files.
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
      case FormDataElement::kEncodedFileSystemURL:
        size += e.file_length_ - e.file_start_;
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
