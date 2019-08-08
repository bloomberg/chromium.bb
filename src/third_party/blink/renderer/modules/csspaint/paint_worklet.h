// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/workers/worklet.h"
#include "third_party/blink/renderer/modules/csspaint/document_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/main_thread_document_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope_proxy.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_pending_generator_registry.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_proxy_client.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

extern DocumentPaintDefinition* const kInvalidDocumentPaintDefinition;

class CSSPaintImageGeneratorImpl;

// Manages a paint worklet:
// https://drafts.css-houdini.org/css-paint-api/#dom-css-paintworklet
class MODULES_EXPORT PaintWorklet : public Worklet,
                                    public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(PaintWorklet);

 public:
  static const char kSupplementName[];

  // At this moment, paint worklet allows at most two global scopes at any time.
  static const wtf_size_t kNumGlobalScopes;
  static PaintWorklet* From(LocalDOMWindow&);

  explicit PaintWorklet(LocalFrame*);
  ~PaintWorklet() override;

  void AddPendingGenerator(const String& name, CSSPaintImageGeneratorImpl*);
  // The |container_size| is without subpixel snapping.
  scoped_refptr<Image> Paint(const String& name,
                             const ImageResourceObserver&,
                             const FloatSize& container_size,
                             const CSSStyleValueVector*);

  int WorkletId() const { return worklet_id_; }
  void Trace(blink::Visitor*) override;

  // Used for main-thread CSS Paint. The DocumentDefinitionMap tracks
  // definitions registered via registerProperty; definitions are only
  // considered valid once all global scopes have registered the same definition
  // for the same thread.
  typedef HeapHashMap<String, Member<DocumentPaintDefinition>>
      DocumentDefinitionMap;
  DocumentDefinitionMap& GetDocumentDefinitionMap() {
    return document_definition_map_;
  }

  // Used for off-thread CSS Paint. In this mode we are not responsible for
  // tracking whether a definition is valid - this method should only be called
  // once all global scopes have registered the same
  // |MainThreadDocumentPaintDefinition| for the same |name|.
  void RegisterMainThreadDocumentPaintDefinition(
      const String& name,
      Vector<CSSPropertyID> native_properties,
      Vector<String> custom_properties,
      double alpha);
  typedef HashMap<String, std::unique_ptr<MainThreadDocumentPaintDefinition>>
      MainThreadDocumentDefinitionMap;
  const MainThreadDocumentDefinitionMap& GetMainThreadDocumentDefinitionMap() {
    return main_thread_document_definition_map_;
  }

 protected:
  // Since paint worklet has more than one global scope, we MUST override this
  // function and provide our own selection logic.
  wtf_size_t SelectGlobalScope() final;
  wtf_size_t GetActiveGlobalScopeForTesting() { return active_global_scope_; }

 private:
  friend class PaintWorkletTest;

  // Implements Worklet.
  bool NeedsToCreateGlobalScope() final;
  WorkletGlobalScopeProxy* CreateGlobalScope() final;

  // This function calculates the number of paints to use before switching
  // global scopes.
  virtual int GetPaintsBeforeSwitching();
  // This function calculates the next global scope to switch to.
  virtual wtf_size_t SelectNewGlobalScope();

  Member<PaintWorkletPendingGeneratorRegistry> pending_generator_registry_;
  DocumentDefinitionMap document_definition_map_;

  // The last document paint frame a paint worklet painted on. This is used to
  // tell when we begin painting on a new frame.
  size_t active_frame_count_ = 0u;
  // The current global scope being used for painting.
  wtf_size_t active_global_scope_ = 0u;
  // The number of paint calls remaining before Paint will select a new global
  // scope. SelectGlobalScope resets this at the beginning of each frame.
  int paints_before_switching_global_scope_;

  // An atomic sequence number to ensure that it is unique for each paint
  // worklet. This id is integrated in the PaintWorkletInput which will be used
  // in PaintWorkletPaintDispatcher::Paint, to identify the right painter, to
  // paint the image.
  int worklet_id_;

  // The proxy client associated with this PaintWorklet. We keep a reference in
  // to ensure that all global scopes get the same proxy client.
  Member<PaintWorkletProxyClient> proxy_client_;

  // Used in off-thread CSS Paint, where it tracks valid definitions.
  HashMap<String, std::unique_ptr<MainThreadDocumentPaintDefinition>>
      main_thread_document_definition_map_;

  DISALLOW_COPY_AND_ASSIGN(PaintWorklet);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_H_
