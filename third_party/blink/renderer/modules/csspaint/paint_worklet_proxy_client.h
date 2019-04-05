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
//
// TODO(smcgruer): Add the dispatcher logic.
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
      scoped_refptr<PaintWorkletPaintDispatcher> compositor_paintee);
  ~PaintWorkletProxyClient() override = default;

  void Trace(blink::Visitor*) override;

  // PaintWorkletPainter implementation
  int GetWorkletId() const override { return worklet_id_; }
  sk_sp<PaintRecord> Paint(CompositorPaintWorkletInput*) override;

  virtual void SetGlobalScope(WorkletGlobalScope*);
  void SetGlobalScopeForTesting(PaintWorkletGlobalScope*);
  void Dispose();

  static PaintWorkletProxyClient* From(WorkerClients*);

 private:
  friend class PaintWorkletGlobalScopeTest;
  friend class PaintWorkletProxyClientTest;
  FRIEND_TEST_ALL_PREFIXES(PaintWorkletProxyClientTest,
                           PaintWorkletProxyClientConstruction);
  FRIEND_TEST_ALL_PREFIXES(PaintWorkletProxyClientTest, SetGlobalScope);

  scoped_refptr<PaintWorkletPaintDispatcher> compositor_paintee_;
  const int worklet_id_;
  CrossThreadPersistent<PaintWorkletGlobalScope> global_scope_;
  enum RunState { kUninitialized, kWorking, kDisposed } state_;
};

void MODULES_EXPORT ProvidePaintWorkletProxyClientTo(WorkerClients*,
                                                     PaintWorkletProxyClient*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_PROXY_CLIENT_H_
