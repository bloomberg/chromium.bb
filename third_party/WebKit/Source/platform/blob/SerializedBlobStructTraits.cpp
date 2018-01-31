// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/blob/SerializedBlobStructTraits.h"

namespace mojo {

bool StructTraits<blink::mojom::blink::SerializedBlob::DataView,
                  scoped_refptr<blink::BlobDataHandle>>::
    Read(blink::mojom::blink::SerializedBlob::DataView data,
         scoped_refptr<blink::BlobDataHandle>* out) {
  WTF::String uuid;
  WTF::String type;
  if (!data.ReadUuid(&uuid) || !data.ReadContentType(&type))
    return false;
  *out = blink::BlobDataHandle::Create(
      uuid, type, data.size(),
      data.TakeBlob<blink::mojom::blink::BlobPtrInfo>());
  return true;
}

}  // namespace mojo
