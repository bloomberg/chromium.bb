// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBRequestLoader_h
#define IDBRequestLoader_h

#include <memory>

#include "core/fileapi/FileReaderLoaderClient.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class FileReaderLoader;
class IDBRequestQueueItem;
class IDBRequest;
class IDBValue;

// Loads IndexedDB values that have been wrapped in Blobs by IDBValueWrapper.
//
// An IDBRequestLoader unwraps the result of a single IDBRequest. While most
// IndexedDB requests result in a single value, getAll() in IDBObjectStore and
// IDBIndex results in an array of values. In the interest of simplicity,
// IDBRequestLoader only knows how to unwrap an array of values, even though
// most of the time the array will consist of a single element. This design
// assumes that the overhead of creating and destroying a Vector is much smaller
// than the IPC overhead required to load the Blob data into the renderer.
class IDBRequestLoader : public FileReaderLoaderClient {
  USING_FAST_MALLOC(IDBRequestLoader);

 public:
  // Creates a loader that will unwrap IDBValues received by a IDBRequest.
  //
  // result_values must be kept alive until the loader calls
  // IDBRequestQueueItem::OnResultLoadComplete().
  IDBRequestLoader(IDBRequestQueueItem*,
                   Vector<RefPtr<IDBValue>>* result_values);

  ~IDBRequestLoader() override;

  // Start unwrapping values.
  //
  // When the unwrapping completes, the loader will call OnResultLoadComplete()
  // on the request queue item.
  void Start();
  // Halt the process of unwrapping values, if possible.
  void Cancel();

  // FileReaderLoaderClient implementaton.
  void DidStartLoading() override;
  void DidReceiveDataForClient(const char* data, unsigned data_length) override;
  void DidFinishLoading() override;
  void DidFail(FileError::ErrorCode) override;

 private:
  // Starts unwrapping the next wrapped IDBValue.
  //
  // If no more wrapped IDBValues are found, this calls ReportSuccess(), which
  // ends up calling IDBRequestQueueItem::OnResultLoadComplete().
  void StartNextValue();

  void ReportSuccess();
  void ReportError();

  std::unique_ptr<FileReaderLoader> loader_;

  // Transaction result queue item for the IDBRequest.
  //
  // The IDBRequestQueueItem owns this loader.
  IDBRequestQueueItem* queue_item_;

  // All the values that will be passed back to the IDBRequest.
  //
  // The Vector is owned by the IDBRequestLoader owner, which is currently a
  // IDBRequestQueueItem.
  Vector<RefPtr<IDBValue>>* const values_;

  // Buffer used to unwrap an IDBValue.
  Vector<char> wrapped_data_;

  // The value being currently unwrapped.
  Vector<RefPtr<IDBValue>>::iterator current_value_;

#if DCHECK_IS_ON()
  // True after Start() is called.
  bool started_ = false;

  // True after Cancel() is called.
  bool canceled_ = false;

  // True between a call to FileReaderLoader::Start() and the FileReaderLoader's
  // call to DidFinishLoading() or to DidFail().
  bool file_reader_loading_ = false;
#endif  // DCHECK_IS_ON()
};

}  // namespace blink

#endif  // IDBRequestLoader_h
