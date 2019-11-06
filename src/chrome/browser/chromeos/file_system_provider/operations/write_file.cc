// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/write_file.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {

WriteFile::WriteFile(extensions::EventRouter* event_router,
                     const ProvidedFileSystemInfo& file_system_info,
                     int file_handle,
                     scoped_refptr<net::IOBuffer> buffer,
                     int64_t offset,
                     int length,
                     storage::AsyncFileUtil::StatusCallback callback)
    : Operation(event_router, file_system_info),
      file_handle_(file_handle),
      buffer_(buffer),
      offset_(offset),
      length_(length),
      callback_(std::move(callback)) {}

WriteFile::~WriteFile() {
}

bool WriteFile::Execute(int request_id) {
  TRACE_EVENT0("file_system_provider", "WriteFile::Execute");
  using extensions::api::file_system_provider::WriteFileRequestedOptions;

  if (!file_system_info_.writable())
    return false;

  WriteFileRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.open_request_id = file_handle_;
  options.offset = offset_;
  // Length is not passed directly since it can be accessed via data.byteLength.

  // Set the data directly on base::Value() to avoid an extra string copy.
  DCHECK(buffer_.get());
  std::unique_ptr<base::DictionaryValue> options_as_value = options.ToValue();
  options_as_value->Set(
      "data", base::Value::CreateWithCopiedBuffer(buffer_->data(), length_));

  std::unique_ptr<base::ListValue> event_args(new base::ListValue);
  event_args->Append(std::move(options_as_value));

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_WRITE_FILE_REQUESTED,
      extensions::api::file_system_provider::OnWriteFileRequested::kEventName,
      std::move(event_args));
}

void WriteFile::OnSuccess(int /* request_id */,
                          std::unique_ptr<RequestValue> /* result */,
                          bool /* has_more */) {
  TRACE_EVENT0("file_system_provider", "WriteFile::OnSuccess");
  DCHECK(callback_);
  std::move(callback_).Run(base::File::FILE_OK);
}

void WriteFile::OnError(int /* request_id */,
                        std::unique_ptr<RequestValue> /* result */,
                        base::File::Error error) {
  TRACE_EVENT0("file_system_provider", "WriteFile::OnError");
  DCHECK(callback_);
  std::move(callback_).Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
