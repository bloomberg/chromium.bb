// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_STORAGE_BUCKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_STORAGE_BUCKET_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ScriptState;

class StorageBucket final : public ScriptWrappable,
                            public ActiveScriptWrappable<StorageBucket>,
                            public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  StorageBucket(ExecutionContext* context,
                mojo::PendingRemote<mojom::blink::BucketHost> remote);

  ~StorageBucket() override = default;

  ScriptPromise persist(ScriptState*);
  ScriptPromise persisted(ScriptState*);
  ScriptPromise estimate(ScriptState*);
  ScriptPromise durability(ScriptState*);
  ScriptPromise setExpires(ScriptState*, const DOMTimeStamp&);
  ScriptPromise expires(ScriptState*);

  // ActiveScriptWrappable
  bool HasPendingActivity() const final;

  // GarbageCollected
  void Trace(Visitor*) const override;

 private:
  void DidRequestPersist(ScriptPromiseResolver* resolver,
                         bool persisted,
                         bool success);
  void DidGetPersisted(ScriptPromiseResolver* resolver,
                       bool persisted,
                       bool success);
  void DidGetEstimate(ScriptPromiseResolver* resolver,
                      int64_t current_usage,
                      int64_t current_quota,
                      bool success);
  void DidGetDurability(ScriptPromiseResolver* resolver,
                        mojom::blink::BucketDurability durability,
                        bool success);
  void DidSetExpires(ScriptPromiseResolver* resolver, bool success);
  void DidGetExpires(ScriptPromiseResolver* resolver,
                     const absl::optional<base::Time> expires,
                     bool success);

  // ExecutionContextLifecycleObserver
  void ContextDestroyed() override;

  // BucketHost in the browser process.
  mojo::Remote<mojom::blink::BucketHost> remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_STORAGE_BUCKET_H_
