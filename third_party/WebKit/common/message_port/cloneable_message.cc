// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/common/message_port/cloneable_message.h"

#include "third_party/WebKit/common/blob/blob.mojom.h"
#include "third_party/WebKit/common/message_port/message_port.mojom.h"

namespace blink {

CloneableMessage::CloneableMessage() = default;
CloneableMessage::CloneableMessage(CloneableMessage&&) = default;
CloneableMessage& CloneableMessage::operator=(CloneableMessage&&) = default;
CloneableMessage::~CloneableMessage() = default;

CloneableMessage CloneableMessage::ShallowClone() const {
  CloneableMessage clone;
  clone.encoded_message = encoded_message;
  for (const auto& blob : blobs) {
    mojom::BlobPtr blob_clone;
    blob->blob->Clone(MakeRequest(&blob_clone));
    clone.blobs.push_back(mojom::SerializedBlob::New(
        blob->uuid, blob->content_type, blob->size, std::move(blob_clone)));
  }
  return clone;
}

}  // namespace blink
