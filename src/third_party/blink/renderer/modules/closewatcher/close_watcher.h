// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLOSEWATCHER_CLOSE_WATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLOSEWATCHER_CLOSE_WATCHER_H_

#include "third_party/blink/public/mojom/close_watcher/close_listener.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class CloseWatcher final : public EventTargetWithInlineData,
                           public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CloseWatcher* Create(ScriptState*, ExceptionState&);
  explicit CloseWatcher(LocalDOMWindow*);
  void Trace(Visitor*) const override;

  bool IsClosed() const { return state_ == State::kClosed; }

  void signalClosed();
  void destroy();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(cancel, kCancel)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close, kClose)

  // EventTargetWithInlineData overrides:
  const AtomicString& InterfaceName() const final;
  ExecutionContext* GetExecutionContext() const final {
    return ExecutionContextClient::GetExecutionContext();
  }

 private:
  void Close();

  // If multiple CloseWatchers are active in a given window, they form a
  // stack, and a close signal will pop the top watcher. If the stack is empty,
  // the first CloseWatcher is "free", but creating a new
  // CloseWatcher when the stack is non-empty requires a user activation.
  class WatcherStack final : public NativeEventListener,
                             public Supplement<LocalDOMWindow>,
                             public mojom::blink::CloseListener {
   public:
    static const char kSupplementName[];

    static WatcherStack& From(LocalDOMWindow&);
    explicit WatcherStack(LocalDOMWindow&);

    void Add(CloseWatcher*);
    void Remove(CloseWatcher*);
    bool HasActiveWatcher() { return !watchers_.IsEmpty(); }

    void Trace(Visitor*) const final;

   private:
    // NativeEventListener override:
    void Invoke(ExecutionContext*, Event*) final;

    // mojom::blink::CloseListener override:
    void Signal() final { watchers_.back()->signalClosed(); }

    HeapLinkedHashSet<Member<CloseWatcher>> watchers_;

    // Holds a pipe which the service uses to notify this object
    // when the idle state has changed.
    HeapMojoReceiver<mojom::blink::CloseListener, WatcherStack> receiver_;
  };

  enum class State { kActive, kModal, kClosed };
  State state_ = State::kActive;
  bool dispatching_cancel_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLOSEWATCHER_CLOSE_WATCHER_H_
