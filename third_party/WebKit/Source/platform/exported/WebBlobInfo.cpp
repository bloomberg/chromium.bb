// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebBlobInfo.h"

#include "platform/blob/BlobData.h"

namespace blink {

WebBlobInfo::WebBlobInfo(const WebString& uuid,
                         const WebString& type,
                         long long size,
                         mojo::ScopedMessagePipeHandle handle)
    : WebBlobInfo(BlobDataHandle::Create(
          uuid,
          type,
          size,
          mojom::blink::BlobPtrInfo(std::move(handle),
                                    mojom::blink::Blob::Version_))) {}

WebBlobInfo::WebBlobInfo(const WebString& uuid,
                         const WebString& file_path,
                         const WebString& file_name,
                         const WebString& type,
                         double last_modified,
                         long long size,
                         mojo::ScopedMessagePipeHandle handle)
    : WebBlobInfo(BlobDataHandle::Create(
                      uuid,
                      type,
                      size,
                      mojom::blink::BlobPtrInfo(std::move(handle),
                                                mojom::blink::Blob::Version_)),
                  file_path,
                  file_name,
                  last_modified) {}

// static
WebBlobInfo WebBlobInfo::BlobForTesting(const WebString& uuid,
                                        const WebString& type,
                                        long long size) {
  return WebBlobInfo(uuid, type, size, mojo::MessagePipe().handle0);
}

// static
WebBlobInfo WebBlobInfo::FileForTesting(const WebString& uuid,
                                        const WebString& file_path,
                                        const WebString& file_name,
                                        const WebString& type) {
  return WebBlobInfo(uuid, file_path, file_name, type, 0, -1,
                     mojo::MessagePipe().handle0);
}

WebBlobInfo::~WebBlobInfo() {
  blob_handle_.Reset();
}

WebBlobInfo::WebBlobInfo(const WebBlobInfo& other) {
  *this = other;
}

WebBlobInfo& WebBlobInfo::operator=(const WebBlobInfo& other) = default;

mojo::ScopedMessagePipeHandle WebBlobInfo::CloneBlobHandle() const {
  if (!blob_handle_)
    return mojo::ScopedMessagePipeHandle();
  return blob_handle_->CloneBlobPtr().PassInterface().PassHandle();
}

WebBlobInfo::WebBlobInfo(scoped_refptr<BlobDataHandle> handle)
    : WebBlobInfo(handle, handle->GetType(), handle->size()) {}

WebBlobInfo::WebBlobInfo(scoped_refptr<BlobDataHandle> handle,
                         const WebString& file_path,
                         const WebString& file_name,
                         double last_modified)
    : WebBlobInfo(handle,
                  file_path,
                  file_name,
                  handle->GetType(),
                  last_modified,
                  handle->size()) {}

WebBlobInfo::WebBlobInfo(scoped_refptr<BlobDataHandle> handle,
                         const WebString& type,
                         long long size)
    : is_file_(false),
      uuid_(handle->Uuid()),
      type_(type),
      size_(size),
      blob_handle_(std::move(handle)),
      last_modified_(0) {}

WebBlobInfo::WebBlobInfo(scoped_refptr<BlobDataHandle> handle,
                         const WebString& file_path,
                         const WebString& file_name,
                         const WebString& type,
                         double last_modified,
                         long long size)
    : is_file_(true),
      uuid_(handle->Uuid()),
      type_(type),
      size_(size),
      blob_handle_(std::move(handle)),
      file_path_(file_path),
      file_name_(file_name),
      last_modified_(last_modified) {}

scoped_refptr<BlobDataHandle> WebBlobInfo::GetBlobHandle() const {
  return blob_handle_.Get();
}

}  // namespace blink
