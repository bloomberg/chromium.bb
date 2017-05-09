// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/MediaKeysClientImpl.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/WebLocalFrameBase.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/web/WebFrameClient.h"

namespace blink {

MediaKeysClientImpl::MediaKeysClientImpl() {}

WebEncryptedMediaClient* MediaKeysClientImpl::EncryptedMediaClient(
    ExecutionContext* execution_context) {
  Document* document = ToDocument(execution_context);
  WebLocalFrameBase* web_frame =
      WebLocalFrameBase::FromFrame(document->GetFrame());
  return web_frame->Client()->EncryptedMediaClient();
}

}  // namespace blink
