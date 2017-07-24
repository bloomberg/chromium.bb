// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintWorklet_h
#define PaintWorklet_h

#include "core/workers/Worklet.h"
#include "modules/ModulesExport.h"
#include "modules/csspaint/DocumentPaintDefinition.h"
#include "modules/csspaint/PaintWorkletGlobalScopeProxy.h"
#include "modules/csspaint/PaintWorkletPendingGeneratorRegistry.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSPaintImageGeneratorImpl;

// Manages a paint worklet:
// https://drafts.css-houdini.org/css-paint-api/#dom-css-paintworklet
class MODULES_EXPORT PaintWorklet final : public Worklet {
  WTF_MAKE_NONCOPYABLE(PaintWorklet);

 public:
  // At this moment, paint worklet allows at most two global scopes at any time.
  static const unsigned kNumGlobalScopes = 2u;
  static PaintWorklet* Create(LocalFrame*);
  ~PaintWorklet() override;

  void AddPendingGenerator(const String& name, CSSPaintImageGeneratorImpl*);
  RefPtr<Image> Paint(const String& name,
                      const ImageResourceObserver&,
                      const IntSize&,
                      const CSSStyleValueVector*);

  typedef HeapHashMap<String, TraceWrapperMember<DocumentPaintDefinition>>
      DocumentDefinitionMap;
  DocumentDefinitionMap& GetDocumentDefinitionMap() {
    return document_definition_map_;
  }
  DECLARE_VIRTUAL_TRACE();

 private:
  friend class PaintWorkletTest;

  explicit PaintWorklet(LocalFrame*);

  // Implements Worklet.
  bool NeedsToCreateGlobalScope() final;
  WorkletGlobalScopeProxy* CreateGlobalScope() final;

  Member<PaintWorkletPendingGeneratorRegistry> pending_generator_registry_;
  DocumentDefinitionMap document_definition_map_;
};

}  // namespace blink

#endif  // PaintWorklet_h
