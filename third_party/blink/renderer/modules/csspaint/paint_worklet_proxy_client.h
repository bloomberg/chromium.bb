// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_PROXY_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_PROXY_CLIENT_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_painter.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"

namespace blink {

class WorkletGlobalScope;

// Mediates between a worklet-thread bound PaintWorkletGlobalScope and its
// associated dispatchers. A PaintWorkletProxyClient is associated with a single
// global scope and one dispatcher to the compositor thread.
//
// This is constructed on the main thread but it is used in the worklet backing
// thread.
class MODULES_EXPORT PaintWorkletProxyClient
    : public GarbageCollectedFinalized<PaintWorkletProxyClient>,
      public Supplement<WorkerClients>,
      public PaintWorkletPainter {
  USING_GARBAGE_COLLECTED_MIXIN(PaintWorkletProxyClient);
  DISALLOW_COPY_AND_ASSIGN(PaintWorkletProxyClient);

 public:
  static const char kSupplementName[];

  static PaintWorkletProxyClient* Create(Document*, int worklet_id);

  PaintWorkletProxyClient(
      int worklet_id,
      PaintWorklet*,
      scoped_refptr<PaintWorkletPaintDispatcher> compositor_paintee);
  ~PaintWorkletProxyClient() override = default;

  void Trace(blink::Visitor*) override;

  // PaintWorkletPainter implementation
  int GetWorkletId() const override { return worklet_id_; }
  sk_sp<PaintRecord> Paint(CompositorPaintWorkletInput*) override;

  virtual void AddGlobalScope(WorkletGlobalScope*);
  const Vector<CrossThreadPersistent<PaintWorkletGlobalScope>>&
  GetGlobalScopesForTesting() const {
    return global_scopes_;
  }

  void RegisterCSSPaintDefinition(const String& name,
                                  CSSPaintDefinition*,
                                  ExceptionState&);

  void Dispose();

  static PaintWorkletProxyClient* From(WorkerClients*);

  const HeapHashMap<String, Member<DocumentPaintDefinition>>&
  DocumentDefinitionMapForTesting() const {
    return document_definition_map_;
  }
  scoped_refptr<base::SingleThreadTaskRunner> MainThreadTaskRunnerForTesting()
      const {
    return main_thread_runner_;
  }
  void SetMainThreadTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> runner) {
    main_thread_runner_ = runner;
  }

 private:
  friend class PaintWorkletGlobalScopeTest;
  friend class PaintWorkletProxyClientTest;
  FRIEND_TEST_ALL_PREFIXES(PaintWorkletProxyClientTest,
                           PaintWorkletProxyClientConstruction);

  scoped_refptr<PaintWorkletPaintDispatcher> compositor_paintee_;
  const int worklet_id_;
  Vector<CrossThreadPersistent<PaintWorkletGlobalScope>> global_scopes_;
  enum RunState { kUninitialized, kWorking, kDisposed } state_;

  // Stores the definitions for each paint that is registered.
  HeapHashMap<String, Member<DocumentPaintDefinition>> document_definition_map_;

  // Used for OffMainThreadPaintWorklet; we post to the main-thread PaintWorklet
  // instance to store the definitions for each paint that is registered.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;
  CrossThreadWeakPersistent<PaintWorklet> paint_worklet_;
};

void MODULES_EXPORT ProvidePaintWorkletProxyClientTo(WorkerClients*,
                                                     PaintWorkletProxyClient*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_PROXY_CLIENT_H_
