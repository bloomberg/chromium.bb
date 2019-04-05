// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_proxy_client.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"

namespace blink {

const char PaintWorkletProxyClient::kSupplementName[] =
    "PaintWorkletProxyClient";

// static
PaintWorkletProxyClient* PaintWorkletProxyClient::Create(Document* document,
                                                         int worklet_id) {
  WebLocalFrameImpl* local_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());

  scoped_refptr<PaintWorkletPaintDispatcher> compositor_painter_dispatcher =
      local_frame->LocalRootFrameWidget()->EnsureCompositorPaintDispatcher();
  return MakeGarbageCollected<PaintWorkletProxyClient>(
      worklet_id, std::move(compositor_painter_dispatcher));
}

PaintWorkletProxyClient::PaintWorkletProxyClient(
    int worklet_id,
    scoped_refptr<PaintWorkletPaintDispatcher> compositor_paintee)
    : compositor_paintee_(std::move(compositor_paintee)),
      worklet_id_(worklet_id),
      state_(RunState::kUninitialized) {
  DCHECK(IsMainThread());
}

void PaintWorkletProxyClient::Trace(blink::Visitor* visitor) {
  Supplement<WorkerClients>::Trace(visitor);
  PaintWorkletPainter::Trace(visitor);
}

void PaintWorkletProxyClient::SetGlobalScopeForTesting(
    PaintWorkletGlobalScope* global_scope) {
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());
  global_scope_ = global_scope;
}

void PaintWorkletProxyClient::SetGlobalScope(WorkletGlobalScope* global_scope) {
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());
  if (state_ == RunState::kDisposed)
    return;
  DCHECK(state_ == RunState::kUninitialized);

  global_scope_ = static_cast<PaintWorkletGlobalScope*>(global_scope);
  scoped_refptr<base::SingleThreadTaskRunner> global_scope_runner =
      global_scope_->GetThread()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  state_ = RunState::kWorking;

  compositor_paintee_->RegisterPaintWorkletPainter(this, global_scope_runner);
}

void PaintWorkletProxyClient::Dispose() {
  if (state_ == RunState::kWorking) {
    compositor_paintee_->UnregisterPaintWorkletPainter(this);

    DCHECK(global_scope_);
    DCHECK(global_scope_->IsContextThread());

    // At worklet scope termination break the reference cycle between
    // PaintWorkletGlobalScope and PaintWorkletProxyClient.
    global_scope_ = nullptr;
  }

  compositor_paintee_ = nullptr;

  DCHECK(state_ != RunState::kDisposed);
  state_ = RunState::kDisposed;
}

sk_sp<PaintRecord> PaintWorkletProxyClient::Paint(
    CompositorPaintWorkletInput* compositor_input) {
  if (!global_scope_)
    return sk_make_sp<PaintRecord>();
  PaintWorkletInput* input = static_cast<PaintWorkletInput*>(compositor_input);
  CSSPaintDefinition* definition =
      global_scope_->FindDefinition(input->NameCopy());

  return definition->Paint(FloatSize(input->GetSize()), input->EffectiveZoom(),
                           nullptr, nullptr);
}

// static
PaintWorkletProxyClient* PaintWorkletProxyClient::From(WorkerClients* clients) {
  return Supplement<WorkerClients>::From<PaintWorkletProxyClient>(clients);
}

void ProvidePaintWorkletProxyClientTo(WorkerClients* clients,
                                      PaintWorkletProxyClient* client) {
  clients->ProvideSupplement(client);
}

}  // namespace blink
