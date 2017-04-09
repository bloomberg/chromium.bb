// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/MediaKeysController.h"

#include "modules/encryptedmedia/MediaKeysClient.h"
#include "public/platform/WebContentDecryptionModule.h"

namespace blink {

const char* MediaKeysController::SupplementName() {
  return "MediaKeysController";
}

MediaKeysController::MediaKeysController(MediaKeysClient* client)
    : client_(client) {}

WebEncryptedMediaClient* MediaKeysController::EncryptedMediaClient(
    ExecutionContext* context) {
  return client_->EncryptedMediaClient(context);
}

void MediaKeysController::ProvideMediaKeysTo(Page& page,
                                             MediaKeysClient* client) {
  MediaKeysController::ProvideTo(page, SupplementName(),
                                 new MediaKeysController(client));
}

}  // namespace blink
