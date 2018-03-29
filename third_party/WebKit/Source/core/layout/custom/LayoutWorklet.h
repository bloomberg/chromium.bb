// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutWorklet_h
#define LayoutWorklet_h

#include "core/CoreExport.h"
#include "core/layout/custom/DocumentLayoutDefinition.h"
#include "core/layout/custom/PendingLayoutRegistry.h"
#include "core/workers/Worklet.h"
#include "platform/heap/Handle.h"

namespace blink {

class Node;
class LayoutWorkletGlobalScopeProxy;

extern DocumentLayoutDefinition* const kInvalidDocumentLayoutDefinition;

// Manages a layout worklet:
// https://drafts.css-houdini.org/css-layout-api/#dom-css-layoutworklet
//
// Provides access to web developer defined layout classes within multiple
// global scopes.
class CORE_EXPORT LayoutWorklet : public Worklet,
                                  public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(LayoutWorklet);

 public:
  static const char kSupplementName[];

  // At the moment, layout worklet allows at most two global scopes at any time.
  static const size_t kNumGlobalScopes;
  static LayoutWorklet* From(LocalDOMWindow&);
  static LayoutWorklet* Create(LocalFrame*);

  ~LayoutWorklet() override;

  typedef HeapHashMap<String, Member<DocumentLayoutDefinition>>
      DocumentDefinitionMap;
  DocumentDefinitionMap* GetDocumentDefinitionMap() {
    return &document_definition_map_;
  }

  void AddPendingLayout(const AtomicString& name, Node*);
  LayoutWorkletGlobalScopeProxy* Proxy();

  void Trace(blink::Visitor*) override;

 protected:
  explicit LayoutWorklet(LocalFrame*);

  // TODO(ikilpatrick): Make selection of the global scope non-deterministic.
  size_t SelectGlobalScope() final { return 0u; }

 private:
  friend class LayoutWorkletTest;

  // Implements Worklet.
  bool NeedsToCreateGlobalScope() final;
  WorkletGlobalScopeProxy* CreateGlobalScope() final;

  DocumentDefinitionMap document_definition_map_;
  Member<PendingLayoutRegistry> pending_layout_registry_;

  DISALLOW_COPY_AND_ASSIGN(LayoutWorklet);
};

}  // namespace blink

#endif  // LayoutWorklet_h
