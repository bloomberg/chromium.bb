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

#ifndef BlobData_h
#define BlobData_h

#include <memory>
#include "base/gtest_prod_util.h"
#include "platform/FileMetadata.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/ThreadSafeRefCounted.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/text/WTFString.h"
#include "storage/public/interfaces/blobs.mojom-blink.h"

namespace blink {

class BlobDataHandle;

class PLATFORM_EXPORT RawData : public ThreadSafeRefCounted<RawData> {
 public:
  static RefPtr<RawData> Create() { return WTF::AdoptRef(new RawData()); }

  const char* data() const { return data_.data(); }
  size_t length() const { return data_.size(); }
  Vector<char>* MutableData() { return &data_; }

 private:
  RawData();

  Vector<char> data_;
};

struct PLATFORM_EXPORT BlobDataItem {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  static const long long kToEndOfFile;

  // Default constructor.
  BlobDataItem()
      : type(kData),
        offset(0),
        length(kToEndOfFile),
        expected_modification_time(InvalidFileTime()) {}

  // Constructor for String type (complete string).
  explicit BlobDataItem(RefPtr<RawData> data)
      : type(kData),
        data(std::move(data)),
        offset(0),
        length(kToEndOfFile),
        expected_modification_time(InvalidFileTime()) {}

  // Constructor for File type (complete file).
  explicit BlobDataItem(const String& path)
      : type(kFile),
        path(path),
        offset(0),
        length(kToEndOfFile),
        expected_modification_time(InvalidFileTime()) {}

  // Constructor for File type (partial file).
  BlobDataItem(const String& path,
               long long offset,
               long long length,
               double expected_modification_time)
      : type(kFile),
        path(path),
        offset(offset),
        length(length),
        expected_modification_time(expected_modification_time) {}

  // Constructor for Blob type.
  BlobDataItem(RefPtr<BlobDataHandle> blob_data_handle,
               long long offset,
               long long length)
      : type(kBlob),
        blob_data_handle(std::move(blob_data_handle)),
        offset(offset),
        length(length),
        expected_modification_time(InvalidFileTime()) {}

  // Constructor for FileSystem file type.
  BlobDataItem(const KURL& file_system_url,
               long long offset,
               long long length,
               double expected_modification_time)
      : type(kFileSystemURL),
        file_system_url(file_system_url),
        offset(offset),
        length(length),
        expected_modification_time(expected_modification_time) {}

  // Detaches from current thread so that it can be passed to another thread.
  void DetachFromCurrentThread();

  const enum { kData, kFile, kBlob, kFileSystemURL } type;

  RefPtr<RawData> data;                   // For Data type.
  String path;                            // For File type.
  KURL file_system_url;                   // For FileSystemURL type.
  RefPtr<BlobDataHandle> blob_data_handle;  // For Blob type.

  long long offset;
  long long length;
  double expected_modification_time;

 private:
  friend class BlobData;

  // Constructor for String type (partial string).
  BlobDataItem(RefPtr<RawData> data, long long offset, long long length)
      : type(kData),
        data(std::move(data)),
        offset(offset),
        length(length),
        expected_modification_time(InvalidFileTime()) {}
};

typedef Vector<BlobDataItem> BlobDataItemList;

class PLATFORM_EXPORT BlobData {
  USING_FAST_MALLOC(BlobData);
  WTF_MAKE_NONCOPYABLE(BlobData);

 public:
  static std::unique_ptr<BlobData> Create();

  // Calling append* on objects returned by createFor___WithUnknownSize will
  // check-fail. The caller can only have an unknown-length file if it is the
  // only item in the blob.
  static std::unique_ptr<BlobData> CreateForFileWithUnknownSize(
      const String& path);
  static std::unique_ptr<BlobData> CreateForFileWithUnknownSize(
      const String& path,
      double expected_modification_time);
  static std::unique_ptr<BlobData> CreateForFileSystemURLWithUnknownSize(
      const KURL& file_system_url,
      double expected_modification_time);

  // Detaches from current thread so that it can be passed to another thread.
  void DetachFromCurrentThread();

  const String& ContentType() const { return content_type_; }
  void SetContentType(const String&);

  const BlobDataItemList& Items() const { return items_; }

  void AppendBytes(const void*, size_t length);
  void AppendData(RefPtr<RawData>, long long offset, long long length);
  void AppendFile(const String& path,
                  long long offset,
                  long long length,
                  double expected_modification_time);

  // The given blob must not be a file with unknown size. Please use the
  // File::appendTo instead.
  void AppendBlob(RefPtr<BlobDataHandle>, long long offset, long long length);
  void AppendFileSystemURL(const KURL&,
                           long long offset,
                           long long length,
                           double expected_modification_time);
  void AppendText(const String&, bool normalize_line_endings_to_native);

  // The value of the size property for a Blob who has this data.
  // BlobDataItem::toEndOfFile if the Blob has a file whose size was not yet
  // determined.
  long long length() const;

  bool IsSingleUnknownSizeFile() const {
    return file_composition_ == FileCompositionStatus::SINGLE_UNKNOWN_SIZE_FILE;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(BlobDataTest, Consolidation);

  enum class FileCompositionStatus {
    SINGLE_UNKNOWN_SIZE_FILE,
    NO_UNKNOWN_SIZE_FILES
  };

  explicit BlobData(FileCompositionStatus composition)
      : file_composition_(composition) {}

  bool CanConsolidateData(size_t length);

  String content_type_;
  FileCompositionStatus file_composition_;
  BlobDataItemList items_;
};

class PLATFORM_EXPORT BlobDataHandle
    : public ThreadSafeRefCounted<BlobDataHandle> {
 public:
  // For empty blob construction.
  static RefPtr<BlobDataHandle> Create() {
    return WTF::AdoptRef(new BlobDataHandle());
  }

  // For initial creation.
  static RefPtr<BlobDataHandle> Create(std::unique_ptr<BlobData> data,
                                       long long size) {
    return WTF::AdoptRef(new BlobDataHandle(std::move(data), size));
  }

  // For deserialization of script values and ipc messages.
  static RefPtr<BlobDataHandle> Create(const String& uuid,
                                       const String& type,
                                       long long size) {
    return WTF::AdoptRef(new BlobDataHandle(uuid, type, size));
  }

  static RefPtr<BlobDataHandle> Create(
      const String& uuid,
      const String& type,
      long long size,
      storage::mojom::blink::BlobPtrInfo blob_info) {
    if (blob_info.is_valid()) {
      return WTF::AdoptRef(
          new BlobDataHandle(uuid, type, size, std::move(blob_info)));
    }
    return WTF::AdoptRef(new BlobDataHandle(uuid, type, size));
  }

  String Uuid() const { return uuid_.IsolatedCopy(); }
  String GetType() const { return type_.IsolatedCopy(); }
  unsigned long long size() const { return size_; }

  bool IsSingleUnknownSizeFile() const { return is_single_unknown_size_file_; }

  ~BlobDataHandle();

  storage::mojom::blink::BlobPtr CloneBlobPtr();

 private:
  BlobDataHandle();
  BlobDataHandle(std::unique_ptr<BlobData>, long long size);
  BlobDataHandle(const String& uuid, const String& type, long long size);
  BlobDataHandle(const String& uuid,
                 const String& type,
                 long long size,
                 storage::mojom::blink::BlobPtrInfo);

  const String uuid_;
  const String type_;
  const long long size_;
  const bool is_single_unknown_size_file_;
  // This class is supposed to be thread safe. So to be able to use the mojo
  // Blob interface from multiple threads store a InterfacePtrInfo combined with
  // a mutex, and make sure any access to the mojo interface is done protected
  // by the mutex.
  storage::mojom::blink::BlobPtrInfo blob_info_;
  Mutex blob_info_mutex_;
};

}  // namespace blink

#endif  // BlobData_h
