// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/messaging/BlinkCloneableMessageStructTraits.h"

#include "platform/blob/BlobData.h"
#include "platform/runtime_enabled_features.h"

namespace mojo {

Vector<blink::mojom::blink::SerializedBlobPtr> StructTraits<
    blink::mojom::blink::CloneableMessage::DataView,
    blink::BlinkCloneableMessage>::blobs(blink::BlinkCloneableMessage& input) {
  Vector<blink::mojom::blink::SerializedBlobPtr> result;
  if (blink::RuntimeEnabledFeatures::MojoBlobsEnabled()) {
    result.ReserveInitialCapacity(input.message->BlobDataHandles().size());
    for (const auto& blob : input.message->BlobDataHandles()) {
      result.push_back(blink::mojom::blink::SerializedBlob::New(
          blob.value->Uuid(), blob.value->GetType(), blob.value->size(),
          blob.value->CloneBlobPtr().PassInterface()));
    }
  }
  return result;
}

bool StructTraits<blink::mojom::blink::CloneableMessage::DataView,
                  blink::BlinkCloneableMessage>::
    Read(blink::mojom::blink::CloneableMessage::DataView data,
         blink::BlinkCloneableMessage* out) {
  mojo::ArrayDataView<uint8_t> message_data;
  data.GetEncodedMessageDataView(&message_data);
  out->message = blink::SerializedScriptValue::Create(
      reinterpret_cast<const char*>(message_data.data()), message_data.size());

  Vector<blink::mojom::blink::SerializedBlobPtr> blobs;
  if (!data.ReadBlobs(&blobs))
    return false;
  for (auto& blob : blobs) {
    out->message->BlobDataHandles().Set(
        blob->uuid,
        blink::BlobDataHandle::Create(blob->uuid, blob->content_type,
                                      blob->size, std::move(blob->blob)));
  }

  return true;
}

}  // namespace mojo
