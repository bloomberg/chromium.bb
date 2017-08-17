// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebBlobRegistry.h"

#include "mojo/public/cpp/bindings/interface_request.h"
#include "platform/wtf/ThreadSpecific.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "storage/public/interfaces/blobs.mojom-blink.h"

namespace blink {

// static
mojo::ScopedMessagePipeHandle WebBlobRegistry::GetBlobPtrFromUUID(
    const WebString& uuid) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<storage::mojom::blink::BlobRegistryPtr>, blob_registry,
      ());
  if (!*blob_registry) {
    // TODO(kinuko): We should use per-frame / per-worker InterfaceProvider
    // instead (crbug.com/734210).
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        MakeRequest(&*blob_registry));
  }
  storage::mojom::blink::BlobPtr blob_ptr;
  (*blob_registry)->GetBlobFromUUID(MakeRequest(&blob_ptr), uuid);
  return blob_ptr.PassInterface().PassHandle();
}

}  // namespace blink
