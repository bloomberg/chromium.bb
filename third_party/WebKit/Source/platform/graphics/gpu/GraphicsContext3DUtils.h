// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GraphicsContext3DUtils_h
#define GraphicsContext3DUtils_h

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/WeakPtr.h"
#include "third_party/skia/include/gpu/GrTexture.h"

namespace blink {

class WebGraphicsContext3DProviderWrapper;

class PLATFORM_EXPORT GraphicsContext3DUtils {
 public:
  // The constructor takes a weak ref to the wrapper because it internally
  // it generates callbacks that may outlive the wrapper.
  GraphicsContext3DUtils(
      WeakPtr<WebGraphicsContext3DProviderWrapper>&& context_provider_wrapper)
      : context_provider_wrapper_(std::move(context_provider_wrapper)) {}

  // Use this service to create a new mailbox or possibly obtain a pre-existing
  // mailbox for a given texture. The caching of pre-existing mailboxes survives
  // when the texture gets recycled by skia for creating a new SkSurface or
  // SkImage with a pre-existing GrTexture backing.
  void GetMailboxForSkImage(gpu::Mailbox&, const sk_sp<SkImage>&);
  void RemoveCachedMailbox(GrTexture*);

 private:
  WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  WTF::HashMap<GrTexture*, gpu::Mailbox> cached_mailboxes_;
};

}  // namespace blink

#endif
