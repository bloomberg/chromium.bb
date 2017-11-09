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
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/UUID.h"
#include "platform/WebTaskRunner.h"
#include "platform/blob/BlobBytesProvider.h"
#include "platform/blob/BlobRegistry.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/runtime_enabled_features.h"
#include "platform/text/LineEnding.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/TextEncoding.h"
#include "public/platform/FilePathConversion.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "third_party/WebKit/common/blob/blob_registry.mojom-blink.h"

namespace blink {

using mojom::blink::BlobPtr;
using mojom::blink::BlobPtrInfo;
using mojom::blink::BlobRegistryPtr;
using mojom::blink::BytesProviderPtr;
using mojom::blink::BytesProviderRequest;
using mojom::blink::DataElement;
using mojom::blink::DataElementBlob;
using mojom::blink::DataElementPtr;
using mojom::blink::DataElementBytes;
using mojom::blink::DataElementBytesPtr;
using mojom::blink::DataElementFile;
using mojom::blink::DataElementFilesystemURL;

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

void BindBytesProvider(std::unique_ptr<BlobBytesProvider> provider,
                       BytesProviderRequest request) {
  mojo::MakeStrongBinding(std::move(provider), std::move(request));
}

BlobRegistryPtr& GetThreadSpecificRegistry() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<BlobRegistryPtr>, registry,
                                  ());
  if (!registry.IsSet()) {
    // TODO(mek): Going through InterfaceProvider to get a BlobRegistryPtr
    // ends up going through the main thread. Ideally workers wouldn't need
    // to do that.
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        MakeRequest(&*registry));
  }
  return *registry;
}

}  // namespace

const long long BlobDataItem::kToEndOfFile = -1;

RawData::RawData() {}

void BlobDataItem::DetachFromCurrentThread() {
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

void BlobData::AppendData(scoped_refptr<RawData> data,
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

void BlobData::AppendBlob(scoped_refptr<BlobDataHandle> data_handle,
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
  scoped_refptr<RawData> data = nullptr;
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
  scoped_refptr<RawData> data = RawData::Create();
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
  if (RuntimeEnabledFeatures::MojoBlobsEnabled()) {
    GetThreadSpecificRegistry()->Register(MakeRequest(&blob_info_), uuid_, "",
                                          "", {});
  } else {
    BlobRegistry::RegisterBlobData(uuid_, BlobData::Create());
  }
}

BlobDataHandle::BlobDataHandle(std::unique_ptr<BlobData> data, long long size)
    : uuid_(CreateCanonicalUUIDString()),
      type_(data->ContentType().IsolatedCopy()),
      size_(size),
      is_single_unknown_size_file_(data->IsSingleUnknownSizeFile()) {
  if (RuntimeEnabledFeatures::MojoBlobsEnabled()) {
    TRACE_EVENT0("Blob", "Registry::RegisterBlob");

    size_t current_memory_population = 0;
    Vector<DataElementPtr> elements;
    const DataElementPtr null_element = nullptr;
    BlobBytesProvider* last_bytes_provider = nullptr;
    scoped_refptr<WebTaskRunner> file_runner =
        Platform::Current()->FileTaskRunner();

    // TODO(mek): When the mojo code path is the default BlobData should
    // directly create mojom::DataElements rather than BlobDataItems,
    // eliminating the need for this loop.
    for (const auto& item : data->Items()) {
      // Skip zero-byte elements, as they don't matter for the contents of
      // the blob.
      if (item.length == 0)
        continue;
      switch (item.type) {
        case BlobDataItem::kData: {
          // kData elements don't set item.length, so separately check for zero
          // byte kData elements.
          if (item.data->length() == 0)
            continue;
          // Since blobs are often constructed with arrays with single bytes,
          // consolidate all adjacent memory blob items into one. This should
          // massively reduce the overhead of describing all these byte
          // elements.
          const DataElementPtr& last_element =
              elements.IsEmpty() ? null_element : elements.back();
          bool should_embed_bytes =
              current_memory_population + item.data->length() <=
              DataElementBytes::kMaximumEmbeddedDataSize;
          bool last_element_is_bytes = last_element && last_element->is_bytes();
          if (last_element_is_bytes) {
            // Append bytes to previous element.
            DCHECK(last_bytes_provider);
            const auto& bytes_element = last_element->get_bytes();
            bytes_element->length += item.data->length();
            if (should_embed_bytes && bytes_element->embedded_data) {
              bytes_element->embedded_data->Append(item.data->data(),
                                                   item.data->length());
              current_memory_population += item.data->length();
            } else if (bytes_element->embedded_data) {
              current_memory_population -= bytes_element->embedded_data->size();
              bytes_element->embedded_data = WTF::nullopt;
            }
            last_bytes_provider->AppendData(item.data);
          } else {
            BytesProviderPtr bytes_provider;
            auto provider = std::make_unique<BlobBytesProvider>(item.data);
            last_bytes_provider = provider.get();
            if (file_runner) {
              // TODO(mek): Considering binding BytesProvider on the IO thread
              // instead, only using the File thread for actual file operations.
              file_runner->PostTask(
                  FROM_HERE,
                  CrossThreadBind(&BindBytesProvider,
                                  WTF::Passed(std::move(provider)),
                                  WTF::Passed(MakeRequest(&bytes_provider))));
            } else {
              BindBytesProvider(std::move(provider),
                                MakeRequest(&bytes_provider));
            }
            DataElementBytesPtr bytes_element = DataElementBytes::New(
                item.data->length(), WTF::nullopt, std::move(bytes_provider));
            if (should_embed_bytes) {
              bytes_element->embedded_data = Vector<uint8_t>();
              bytes_element->embedded_data->Append(item.data->data(),
                                                   item.data->length());
              current_memory_population += item.data->length();
            }
            elements.push_back(DataElement::NewBytes(std::move(bytes_element)));
          }
          break;
        }
        case BlobDataItem::kFile:
          elements.push_back(DataElement::NewFile(DataElementFile::New(
              WebStringToFilePath(item.path), item.offset, item.length,
              WTF::Time::FromDoubleT(item.expected_modification_time))));
          break;
        case BlobDataItem::kFileSystemURL:
          elements.push_back(
              DataElement::NewFileFilesystem(DataElementFilesystemURL::New(
                  item.file_system_url, item.offset, item.length,
                  WTF::Time::FromDoubleT(item.expected_modification_time))));
          break;
        case BlobDataItem::kBlob: {
          BlobPtr blob_clone = item.blob_data_handle->CloneBlobPtr();
          elements.push_back(DataElement::NewBlob(DataElementBlob::New(
              std::move(blob_clone), item.offset, item.length)));
          break;
        }
      }
    }

    GetThreadSpecificRegistry()->Register(MakeRequest(&blob_info_), uuid_,
                                          type_.IsNull() ? "" : type_, "",
                                          std::move(elements));
  } else {
    BlobRegistry::RegisterBlobData(uuid_, std::move(data));
  }
}

BlobDataHandle::BlobDataHandle(const String& uuid,
                               const String& type,
                               long long size)
    : uuid_(uuid.IsolatedCopy()),
      type_(IsValidBlobType(type) ? type.IsolatedCopy() : ""),
      size_(size),
      is_single_unknown_size_file_(false) {
  if (RuntimeEnabledFeatures::MojoBlobsEnabled()) {
    SCOPED_BLINK_UMA_HISTOGRAM_TIMER_THREAD_SAFE(
        "Storage.Blob.GetBlobFromUUIDTime");
    GetThreadSpecificRegistry()->GetBlobFromUUID(MakeRequest(&blob_info_),
                                                 uuid_);
  } else {
    BlobRegistry::AddBlobDataRef(uuid_);
  }
}

BlobDataHandle::BlobDataHandle(const String& uuid,
                               const String& type,
                               long long size,
                               BlobPtrInfo blob_info)
    : uuid_(uuid.IsolatedCopy()),
      type_(IsValidBlobType(type) ? type.IsolatedCopy() : ""),
      size_(size),
      is_single_unknown_size_file_(false),
      blob_info_(std::move(blob_info)) {
  DCHECK(RuntimeEnabledFeatures::MojoBlobsEnabled());
  DCHECK(blob_info_.is_valid());
}

BlobDataHandle::~BlobDataHandle() {
  if (!RuntimeEnabledFeatures::MojoBlobsEnabled())
    BlobRegistry::RemoveBlobDataRef(uuid_);
}

BlobPtr BlobDataHandle::CloneBlobPtr() {
  MutexLocker locker(blob_info_mutex_);
  if (!blob_info_.is_valid())
    return nullptr;
  BlobPtr blob, blob_clone;
  blob.Bind(std::move(blob_info_));
  blob->Clone(MakeRequest(&blob_clone));
  blob_info_ = blob.PassInterface();
  return blob_clone;
}

}  // namespace blink
