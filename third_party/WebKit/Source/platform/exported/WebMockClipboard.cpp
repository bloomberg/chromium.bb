// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebMockClipboard.h"

#include "platform/blob/BlobData.h"

namespace blink {

WebBlobInfo WebMockClipboard::CreateBlobFromData(base::span<const uint8_t> data,
                                                 const WebString& mime_type) {
  auto blob_data = BlobData::Create();
  blob_data->SetContentType(mime_type);
  blob_data->AppendBytes(data.data(), data.size());
  auto blob_data_handle =
      BlobDataHandle::Create(std::move(blob_data), data.size());
  return WebBlobInfo(
      blob_data_handle->Uuid(), mime_type, data.size(),
      blob_data_handle->CloneBlobPtr().PassInterface().PassHandle());
}

}  // namespace blink
