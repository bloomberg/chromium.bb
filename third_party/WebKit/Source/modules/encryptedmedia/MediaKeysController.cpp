// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/MediaKeysController.h"

#include "core/dom/Document.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/web/WebFrameClient.h"

namespace blink {

const char* MediaKeysController::SupplementName() {
  return "MediaKeysController";
}

MediaKeysController::MediaKeysController() {}

WebEncryptedMediaClient* MediaKeysController::EncryptedMediaClient(
    ExecutionContext* context) {
  Document* document = ToDocument(context);
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());
  return web_frame->Client()->EncryptedMediaClient();
}

void MediaKeysController::ProvideMediaKeysTo(Page& page) {
  MediaKeysController::ProvideTo(page, SupplementName(),
                                 new MediaKeysController());
}

}  // namespace blink
