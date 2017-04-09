/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "public/platform/WebBlobData.h"

#include "platform/blob/BlobData.h"
#include <memory>

namespace blink {

WebBlobData::WebBlobData() {}

WebBlobData::~WebBlobData() {}

size_t WebBlobData::ItemCount() const {
  ASSERT(!IsNull());
  return private_->Items().size();
}

bool WebBlobData::ItemAt(size_t index, Item& result) const {
  ASSERT(!IsNull());

  if (index >= private_->Items().size())
    return false;

  const BlobDataItem& item = private_->Items()[index];
  result.data.Reset();
  result.file_path.Reset();
  result.blob_uuid.Reset();
  result.offset = item.offset;
  result.length = item.length;
  result.expected_modification_time = item.expected_modification_time;

  switch (item.type) {
    case BlobDataItem::kData:
      result.type = Item::kTypeData;
      result.data = item.data;
      return true;
    case BlobDataItem::kFile:
      result.type = Item::kTypeFile;
      result.file_path = item.path;
      return true;
    case BlobDataItem::kBlob:
      result.type = Item::kTypeBlob;
      result.blob_uuid = item.blob_data_handle->Uuid();
      return true;
    case BlobDataItem::kFileSystemURL:
      result.type = Item::kTypeFileSystemURL;
      result.file_system_url = item.file_system_url;
      return true;
  }
  ASSERT_NOT_REACHED();
  return false;
}

WebString WebBlobData::ContentType() const {
  ASSERT(!IsNull());
  return private_->ContentType();
}

WebBlobData::WebBlobData(std::unique_ptr<BlobData> data)
    : private_(std::move(data)) {}

WebBlobData& WebBlobData::operator=(std::unique_ptr<BlobData> data) {
  private_ = std::move(data);
  return *this;
}

WebBlobData::operator std::unique_ptr<BlobData>() {
  return std::move(private_);
}

}  // namespace blink
