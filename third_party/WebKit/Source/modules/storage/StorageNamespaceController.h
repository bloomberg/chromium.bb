// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StorageNamespaceController_h
#define StorageNamespaceController_h

#include "core/page/Page.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include <memory>

namespace blink {

class InspectorDOMStorageAgent;
class StorageClient;
class StorageNamespace;

class MODULES_EXPORT StorageNamespaceController final
    : public GarbageCollectedFinalized<StorageNamespaceController>,
      public Supplement<Page> {
  USING_GARBAGE_COLLECTED_MIXIN(StorageNamespaceController);

 public:
  StorageNamespace* SessionStorage(bool optional_create = true);
  StorageClient* GetStorageClient() { return client_; }
  ~StorageNamespaceController();

  static void ProvideStorageNamespaceTo(Page&, StorageClient*);
  static StorageNamespaceController* From(Page* page) {
    return static_cast<StorageNamespaceController*>(
        Supplement<Page>::From(page, SupplementName()));
  }

  DECLARE_TRACE();

  InspectorDOMStorageAgent* InspectorAgent() { return inspector_agent_; }
  void SetInspectorAgent(InspectorDOMStorageAgent* agent) {
    inspector_agent_ = agent;
  }

 private:
  explicit StorageNamespaceController(StorageClient*);
  static const char* SupplementName();
  std::unique_ptr<StorageNamespace> session_storage_;
  StorageClient* client_;
  Member<InspectorDOMStorageAgent> inspector_agent_;
};

}  // namespace blink

#endif  // StorageNamespaceController_h
