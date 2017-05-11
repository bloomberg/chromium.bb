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

#include "platform/blob/BlobData.h"

#include <memory>
#include "platform/UUID.h"
#include "platform/blob/BlobRegistry.h"
#include "platform/text/LineEnding.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/TextEncoding.h"

namespace blink {

namespace {

// All consecutive items that are accumulate to < this number will have the
// data appended to the same item.
static const size_t kMaxConsolidatedItemSizeInBytes = 15 * 1024;

// http://dev.w3.org/2006/webapi/FileAPI/#constructorBlob
bool IsValidBlobType(const String& type) {
  for (unsigned i = 0; i < type.length(); ++i) {
    UChar c = type[i];
    if (c < 0x20 || c > 0x7E)
      return false;
  }
  return true;
}

}  // namespace

const long long BlobDataItem::kToEndOfFile = -1;

RawData::RawData() {}

void RawData::DetachFromCurrentThread() {}

void BlobDataItem::DetachFromCurrentThread() {
  data->DetachFromCurrentThread();
  path = path.IsolatedCopy();
  file_system_url = file_system_url.Copy();
}

std::unique_ptr<BlobData> BlobData::Create() {
  return WTF::WrapUnique(
      new BlobData(FileCompositionStatus::NO_UNKNOWN_SIZE_FILES));
}

std::unique_ptr<BlobData> BlobData::CreateForFileWithUnknownSize(
    const String& path) {
  std::unique_ptr<BlobData> data = WTF::WrapUnique(
      new BlobData(FileCompositionStatus::SINGLE_UNKNOWN_SIZE_FILE));
  data->items_.push_back(BlobDataItem(path));
  return data;
}

std::unique_ptr<BlobData> BlobData::CreateForFileWithUnknownSize(
    const String& path,
    double expected_modification_time) {
  std::unique_ptr<BlobData> data = WTF::WrapUnique(
      new BlobData(FileCompositionStatus::SINGLE_UNKNOWN_SIZE_FILE));
  data->items_.push_back(BlobDataItem(path, 0, BlobDataItem::kToEndOfFile,
                                      expected_modification_time));
  return data;
}

std::unique_ptr<BlobData> BlobData::CreateForFileSystemURLWithUnknownSize(
    const KURL& file_system_url,
    double expected_modification_time) {
  std::unique_ptr<BlobData> data = WTF::WrapUnique(
      new BlobData(FileCompositionStatus::SINGLE_UNKNOWN_SIZE_FILE));
  data->items_.push_back(BlobDataItem(file_system_url, 0,
                                      BlobDataItem::kToEndOfFile,
                                      expected_modification_time));
  return data;
}

void BlobData::DetachFromCurrentThread() {
  content_type_ = content_type_.IsolatedCopy();
  for (size_t i = 0; i < items_.size(); ++i)
    items_.at(i).DetachFromCurrentThread();
}

void BlobData::SetContentType(const String& content_type) {
  if (IsValidBlobType(content_type))
    content_type_ = content_type;
  else
    content_type_ = "";
}

void BlobData::AppendData(PassRefPtr<RawData> data,
                          long long offset,
                          long long length) {
  DCHECK_EQ(file_composition_, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  items_.push_back(BlobDataItem(std::move(data), offset, length));
}

void BlobData::AppendFile(const String& path,
                          long long offset,
                          long long length,
                          double expected_modification_time) {
  DCHECK_EQ(file_composition_, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  DCHECK_NE(length, BlobDataItem::kToEndOfFile)
      << "It is illegal to append file items that have an unknown size. To "
         "create a blob with a single file with unknown size, use "
         "BlobData::createForFileWithUnknownSize. Otherwise please provide the "
         "file size.";
  items_.push_back(
      BlobDataItem(path, offset, length, expected_modification_time));
}

void BlobData::AppendBlob(PassRefPtr<BlobDataHandle> data_handle,
                          long long offset,
                          long long length) {
  DCHECK_EQ(file_composition_, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  DCHECK(!data_handle->IsSingleUnknownSizeFile())
      << "It is illegal to append an unknown size file blob.";
  items_.push_back(BlobDataItem(std::move(data_handle), offset, length));
}

void BlobData::AppendFileSystemURL(const KURL& url,
                                   long long offset,
                                   long long length,
                                   double expected_modification_time) {
  DCHECK_EQ(file_composition_, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  items_.push_back(
      BlobDataItem(url, offset, length, expected_modification_time));
}

void BlobData::AppendText(const String& text,
                          bool do_normalize_line_endings_to_native) {
  DCHECK_EQ(file_composition_, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  CString utf8_text =
      UTF8Encoding().Encode(text, WTF::kEntitiesForUnencodables);
  RefPtr<RawData> data = nullptr;
  Vector<char>* buffer;
  if (CanConsolidateData(text.length())) {
    buffer = items_.back().data->MutableData();
  } else {
    data = RawData::Create();
    buffer = data->MutableData();
  }

  if (do_normalize_line_endings_to_native) {
    NormalizeLineEndingsToNative(utf8_text, *buffer);
  } else {
    buffer->Append(utf8_text.data(), utf8_text.length());
  }

  if (data)
    items_.push_back(BlobDataItem(std::move(data)));
}

void BlobData::AppendBytes(const void* bytes, size_t length) {
  DCHECK_EQ(file_composition_, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  if (CanConsolidateData(length)) {
    items_.back().data->MutableData()->Append(static_cast<const char*>(bytes),
                                              length);
    return;
  }
  RefPtr<RawData> data = RawData::Create();
  Vector<char>* buffer = data->MutableData();
  buffer->Append(static_cast<const char*>(bytes), length);
  items_.push_back(BlobDataItem(std::move(data)));
}

long long BlobData::length() const {
  long long length = 0;

  for (Vector<BlobDataItem>::const_iterator it = items_.begin();
       it != items_.end(); ++it) {
    const BlobDataItem& item = *it;
    if (item.length != BlobDataItem::kToEndOfFile) {
      DCHECK_GE(item.length, 0);
      length += item.length;
      continue;
    }

    switch (item.type) {
      case BlobDataItem::kData:
        length += item.data->length();
        break;
      case BlobDataItem::kFile:
      case BlobDataItem::kBlob:
      case BlobDataItem::kFileSystemURL:
        return BlobDataItem::kToEndOfFile;
    }
  }
  return length;
}

bool BlobData::CanConsolidateData(size_t length) {
  if (items_.IsEmpty())
    return false;
  BlobDataItem& last_item = items_.back();
  if (last_item.type != BlobDataItem::kData)
    return false;
  if (last_item.data->length() + length > kMaxConsolidatedItemSizeInBytes)
    return false;
  return true;
}

BlobDataHandle::BlobDataHandle()
    : uuid_(CreateCanonicalUUIDString()),
      size_(0),
      is_single_unknown_size_file_(false) {
  BlobRegistry::RegisterBlobData(uuid_, BlobData::Create());
}

BlobDataHandle::BlobDataHandle(std::unique_ptr<BlobData> data, long long size)
    : uuid_(CreateCanonicalUUIDString()),
      type_(data->ContentType().IsolatedCopy()),
      size_(size),
      is_single_unknown_size_file_(data->IsSingleUnknownSizeFile()) {
  BlobRegistry::RegisterBlobData(uuid_, std::move(data));
}

BlobDataHandle::BlobDataHandle(const String& uuid,
                               const String& type,
                               long long size)
    : uuid_(uuid.IsolatedCopy()),
      type_(IsValidBlobType(type) ? type.IsolatedCopy() : ""),
      size_(size),
      is_single_unknown_size_file_(false) {
  BlobRegistry::AddBlobDataRef(uuid_);
}

BlobDataHandle::~BlobDataHandle() {
  BlobRegistry::RemoveBlobDataRef(uuid_);
}

}  // namespace blink
