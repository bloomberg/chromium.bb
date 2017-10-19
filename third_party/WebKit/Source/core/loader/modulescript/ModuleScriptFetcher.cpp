// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleScriptFetcher.h"

namespace blink {

void ModuleScriptFetcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(client_);
}

void ModuleScriptFetcher::NotifyFetchFinished(
    const WTF::Optional<ModuleScriptCreationParams>& params,
    ConsoleMessage* error_message) {
  client_->NotifyFetchFinished(params, error_message);
}

void ModuleScriptFetcher::SetClient(Client* client) {
  DCHECK(!client_);
  client_ = client;
}

}  // namespace blink
