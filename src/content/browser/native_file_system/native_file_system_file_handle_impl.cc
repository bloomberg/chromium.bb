// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_file_handle_impl.h"

#include "base/guid.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom.h"

using blink::mojom::NativeFileSystemError;
using storage::BlobDataHandle;
using storage::BlobImpl;
using storage::FileSystemOperation;

namespace content {

struct NativeFileSystemFileHandleImpl::WriteState {
  WriteCallback callback;
  uint64_t bytes_written = 0;
};

NativeFileSystemFileHandleImpl::NativeFileSystemFileHandleImpl(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    storage::IsolatedContext::ScopedFSHandle file_system)
    : NativeFileSystemHandleBase(manager,
                                 context,
                                 url,
                                 std::move(file_system)) {}

NativeFileSystemFileHandleImpl::~NativeFileSystemFileHandleImpl() = default;

void NativeFileSystemFileHandleImpl::AsBlob(AsBlobCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mek): Check backend::SupportsStreaming and create snapshot file if
  // streaming is not supported.
  operation_runner()->GetMetadata(
      url(),
      FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          FileSystemOperation::GET_METADATA_FIELD_SIZE |
          FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
      base::BindOnce(&NativeFileSystemFileHandleImpl::DidGetMetaDataForBlob,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NativeFileSystemFileHandleImpl::Remove(RemoveCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  operation_runner()->RemoveFile(
      url(), base::BindOnce(
                 [](RemoveCallback callback, base::File::Error result) {
                   std::move(callback).Run(NativeFileSystemError::New(result));
                 },
                 std::move(callback)));
}

void NativeFileSystemFileHandleImpl::Write(uint64_t offset,
                                           blink::mojom::BlobPtr data,
                                           WriteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  blob_context()->GetBlobDataFromBlobPtr(
      std::move(data),
      base::BindOnce(&NativeFileSystemFileHandleImpl::DoWriteBlob,
                     weak_factory_.GetWeakPtr(), std::move(callback), offset));
}

void NativeFileSystemFileHandleImpl::WriteStream(
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle stream,
    WriteStreamCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // FileSystemOperationRunner assumes that positions passed to Write are always
  // valid, and will NOTREACHED() if that is not the case, so first check the
  // size of the file to make sure the position passed in from the renderer is
  // in fact valid.
  // Of course the file could still change between checking its size and the
  // write operation being started, but this is at least a lot better than the
  // old implementation where the renderer only checks against how big it thinks
  // the file currently is.
  // TODO(https://crbug.com/957214): Fix this situation.
  operation_runner()->GetMetadata(
      url(), FileSystemOperation::GET_METADATA_FIELD_SIZE,
      base::BindOnce(&NativeFileSystemFileHandleImpl::DoWriteStreamWithFileInfo,
                     weak_factory_.GetWeakPtr(), std::move(callback), offset,
                     std::move(stream)));
}

void NativeFileSystemFileHandleImpl::Truncate(uint64_t length,
                                              TruncateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  operation_runner()->Truncate(
      url(), length,
      base::BindOnce(
          [](TruncateCallback callback, base::File::Error result) {
            std::move(callback).Run(NativeFileSystemError::New(result));
          },
          std::move(callback)));
}

void NativeFileSystemFileHandleImpl::Transfer(
    blink::mojom::NativeFileSystemTransferTokenRequest token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  manager()->CreateTransferToken(*this, std::move(token));
}

void NativeFileSystemFileHandleImpl::DidGetMetaDataForBlob(
    AsBlobCallback callback,
    base::File::Error result,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(NativeFileSystemError::New(result), nullptr);
    return;
  }

  std::string uuid = base::GenerateGUID();
  auto blob_builder = std::make_unique<storage::BlobDataBuilder>(uuid);
  // Use AppendFileSystemFile here, since we're streaming the file directly
  // from the file system backend, and the file thus might not actually be
  // backed by a file on disk.
  blob_builder->AppendFileSystemFile(url().ToGURL(), 0, -1, info.last_modified,
                                     file_system_context());
  std::unique_ptr<BlobDataHandle> blob_handle =
      blob_context()->AddFinishedBlob(std::move(blob_builder));
  if (blob_handle->IsBroken()) {
    std::move(callback).Run(
        NativeFileSystemError::New(base::File::FILE_ERROR_FAILED), nullptr);
    return;
  }

  blink::mojom::BlobPtr blob_ptr;
  BlobImpl::Create(std::move(blob_handle), mojo::MakeRequest(&blob_ptr));

  // TODO(mek): Figure out what mime-type to use for these files. The old
  // file system API implementation uses net::GetWellKnownMimeTypeFromExtension
  // to figure out a reasonable mime type based on the extension.
  std::move(callback).Run(
      NativeFileSystemError::New(base::File::FILE_OK),
      blink::mojom::SerializedBlob::New(uuid, "application/octet-stream",
                                        info.size, blob_ptr.PassInterface()));
}

void NativeFileSystemFileHandleImpl::DoWriteBlob(
    WriteCallback callback,
    uint64_t position,
    std::unique_ptr<BlobDataHandle> blob) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!blob) {
    std::move(callback).Run(
        NativeFileSystemError::New(base::File::FILE_ERROR_FAILED), 0);
    return;
  }

  // FileSystemOperationRunner assumes that positions passed to Write are always
  // valid, and will NOTREACHED() if that is not the case, so first check the
  // size of the file to make sure the position passed in from the renderer is
  // in fact valid.
  // Of course the file could still change between checking its size and the
  // write operation being started, but this is at least a lot better than the
  // old implementation where the renderer only checks against how big it thinks
  // the file currently is.
  // TODO(https://crbug.com/957214): Fix this situation.
  operation_runner()->GetMetadata(
      url(), FileSystemOperation::GET_METADATA_FIELD_SIZE,
      base::BindOnce(&NativeFileSystemFileHandleImpl::DoWriteBlobWithFileInfo,
                     weak_factory_.GetWeakPtr(), std::move(callback), position,
                     std::move(blob)));
}

void NativeFileSystemFileHandleImpl::DoWriteBlobWithFileInfo(
    WriteCallback callback,
    uint64_t position,
    std::unique_ptr<BlobDataHandle> blob,
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (file_info.size < 0 || position > static_cast<uint64_t>(file_info.size)) {
    std::move(callback).Run(
        NativeFileSystemError::New(base::File::FILE_ERROR_FAILED), 0);
    return;
  }

  operation_runner()->Write(
      url(), std::move(blob), position,
      base::BindRepeating(&NativeFileSystemFileHandleImpl::DidWrite,
                          weak_factory_.GetWeakPtr(),
                          base::Owned(new WriteState{std::move(callback)})));
}

void NativeFileSystemFileHandleImpl::DoWriteStreamWithFileInfo(
    WriteStreamCallback callback,
    uint64_t position,
    mojo::ScopedDataPipeConsumerHandle data_pipe,
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (file_info.size < 0 || position > static_cast<uint64_t>(file_info.size)) {
    std::move(callback).Run(
        NativeFileSystemError::New(base::File::FILE_ERROR_FAILED), 0);
    return;
  }

  operation_runner()->Write(
      url(), std::move(data_pipe), position,
      base::BindRepeating(&NativeFileSystemFileHandleImpl::DidWrite,
                          weak_factory_.GetWeakPtr(),
                          base::Owned(new WriteState{std::move(callback)})));
}

void NativeFileSystemFileHandleImpl::DidWrite(WriteState* state,
                                              base::File::Error result,
                                              int64_t bytes,
                                              bool complete) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(state);
  state->bytes_written += bytes;
  if (complete) {
    std::move(state->callback)
        .Run(NativeFileSystemError::New(result), state->bytes_written);
  }
}

}  // namespace content