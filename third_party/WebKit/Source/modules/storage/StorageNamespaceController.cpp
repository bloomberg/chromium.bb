// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/storage/StorageNamespaceController.h"

#include "modules/storage/InspectorDOMStorageAgent.h"
#include "modules/storage/StorageClient.h"
#include "modules/storage/StorageNamespace.h"

namespace blink {

const char* StorageNamespaceController::SupplementName() {
  return "StorageNamespaceController";
}

StorageNamespaceController::StorageNamespaceController(StorageClient* client)
    : client_(client), inspector_agent_(nullptr) {}

StorageNamespaceController::~StorageNamespaceController() {}

DEFINE_TRACE(StorageNamespaceController) {
  Supplement<Page>::Trace(visitor);
  visitor->Trace(inspector_agent_);
}

StorageNamespace* StorageNamespaceController::SessionStorage(
    bool optional_create) {
  if (!session_storage_ && optional_create)
    session_storage_ = client_->CreateSessionStorageNamespace();
  return session_storage_.get();
}

void StorageNamespaceController::ProvideStorageNamespaceTo(
    Page& page,
    StorageClient* client) {
  StorageNamespaceController::ProvideTo(page, SupplementName(),
                                        new StorageNamespaceController(client));
}

}  // namespace blink
