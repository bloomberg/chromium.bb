// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBRequestLoader.h"

#include "core/dom/DOMException.h"
#include "core/fileapi/FileReaderLoader.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBRequestQueueItem.h"
#include "modules/indexeddb/IDBValue.h"
#include "modules/indexeddb/IDBValueWrapping.h"
#include "public/platform/modules/indexeddb/WebIDBDatabaseException.h"

namespace blink {

IDBRequestLoader::IDBRequestLoader(
    IDBRequestQueueItem* queue_item,
    Vector<scoped_refptr<IDBValue>>* result_values)
    : queue_item_(queue_item), values_(result_values) {
  DCHECK(IDBValueUnwrapper::IsWrapped(*values_));
  loader_ = FileReaderLoader::Create(FileReaderLoader::kReadByClient, this);
}

IDBRequestLoader::~IDBRequestLoader() {
  // TODO(pwnall): Do we need to call loader_->Cancel() here?
}

void IDBRequestLoader::Start() {
#if DCHECK_IS_ON()
  DCHECK(!started_) << "Start() was already called";
  started_ = true;
#endif  // DCHECK_IS_ON()

  // TODO(pwnall): Start() / StartNextValue() unwrap large values sequentially.
  //               Consider parallelizing. The main issue is that the Blob reads
  //               will have to be throttled somewhere, and the extra complexity
  //               only benefits applications that use getAll().
  current_value_ = values_->begin();
  StartNextValue();
}

void IDBRequestLoader::Cancel() {
#if DCHECK_IS_ON()
  DCHECK(started_) << "Cancel() called on a loader that hasn't been Start()ed";
  DCHECK(!canceled_) << "Cancel() was already called";
  canceled_ = true;

  DCHECK(file_reader_loading_);
  file_reader_loading_ = false;
#endif  // DCHECK_IS_ON()
  loader_->Cancel();
}

void IDBRequestLoader::StartNextValue() {
  IDBValueUnwrapper unwrapper;

  while (true) {
    if (current_value_ == values_->end()) {
      ReportSuccess();
      return;
    }
    if (unwrapper.Parse(current_value_->get()))
      break;
    ++current_value_;
  }

  DCHECK(current_value_ != values_->end());

  ExecutionContext* context = queue_item_->Request()->GetExecutionContext();
  if (!context) {
    ReportError();
    return;
  }

  wrapped_data_.ReserveCapacity(unwrapper.WrapperBlobSize());
#if DCHECK_IS_ON()
  DCHECK(!file_reader_loading_);
  file_reader_loading_ = true;
#endif  // DCHECK_IS_ON()
  loader_->Start(context, unwrapper.WrapperBlobHandle());
}

void IDBRequestLoader::DidStartLoading() {}

void IDBRequestLoader::DidReceiveDataForClient(const char* data,
                                               unsigned data_length) {
  DCHECK_LE(wrapped_data_.size() + data_length, wrapped_data_.capacity())
      << "The reader returned more data than we were prepared for";

  wrapped_data_.Append(data, data_length);
}

void IDBRequestLoader::DidFinishLoading() {
#if DCHECK_IS_ON()
  DCHECK(started_)
      << "FileReaderLoader called DidFinishLoading() before it was Start()ed";
  DCHECK(!canceled_)
      << "FileReaderLoader called DidFinishLoading() after it was Cancel()ed";

  DCHECK(file_reader_loading_);
  file_reader_loading_ = false;
#endif  // DCHECK_IS_ON()

  *current_value_ = IDBValueUnwrapper::Unwrap(
      current_value_->get(), SharedBuffer::AdoptVector(wrapped_data_));
  ++current_value_;

  StartNextValue();
}

void IDBRequestLoader::DidFail(FileError::ErrorCode) {
#if DCHECK_IS_ON()
  DCHECK(started_)
      << "FileReaderLoader called DidFail() before it was Start()ed";
  DCHECK(!canceled_)
      << "FileReaderLoader called DidFail() after it was Cancel()ed";

  DCHECK(file_reader_loading_);
  file_reader_loading_ = false;
#endif  // DCHECK_IS_ON()

  ReportError();
}

void IDBRequestLoader::ReportSuccess() {
#if DCHECK_IS_ON()
  DCHECK(started_);
  DCHECK(!canceled_);
#endif  // DCHECK_IS_ON()
  queue_item_->OnResultLoadComplete();
}

void IDBRequestLoader::ReportError() {
#if DCHECK_IS_ON()
  DCHECK(started_);
  DCHECK(!canceled_);
#endif  // DCHECK_IS_ON()
  queue_item_->OnResultLoadComplete(
      DOMException::Create(kDataError, "Failed to read large IndexedDB value"));
}

}  // namespace blink
