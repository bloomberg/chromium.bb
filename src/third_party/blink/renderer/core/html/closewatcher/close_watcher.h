// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CLOSEWATCHER_CLOSE_WATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CLOSEWATCHER_CLOSE_WATCHER_H_

#include "base/time/time.h"
#include "third_party/blink/public/mojom/close_watcher/close_listener.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

class CloseWatcherOptions;
class LocalDOMWindow;
class KeyboardEvent;

class CloseWatcher final : public EventTargetWithInlineData,
                           public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CloseWatcher* Create(ScriptState*,
                              CloseWatcherOptions*,
                              ExceptionState&);

  // TODO(domenic): remove the CloseWatcherOptions* from this overload. This is
  // meant to be used by Chromium-internal callers, who will not want to pass an
  // AbortSignal.
  static CloseWatcher* Create(LocalDOMWindow*, CloseWatcherOptions*);

  explicit CloseWatcher(LocalDOMWindow*);
  void Trace(Visitor*) const override;

  bool IsClosed() const { return state_ == State::kClosed; }
  bool IsGroupedWithPrevious() const { return grouped_with_previous_; }

  void close();
  void destroy();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(cancel, kCancel)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close, kClose)

  // EventTargetWithInlineData overrides:
  const AtomicString& InterfaceName() const final;
  ExecutionContext* GetExecutionContext() const final {
    return ExecutionContextClient::GetExecutionContext();
  }

  // If multiple CloseWatchers are active in a given window, they form a
  // stack, and a close signal will pop the top watcher. If the stack is empty,
  // the first CloseWatcher is "free", but creating a new
  // CloseWatcher when the stack is non-empty requires a user activation.
  class WatcherStack final : public GarbageCollected<WatcherStack>,
                             public mojom::blink::CloseListener {
   public:
    explicit WatcherStack(LocalDOMWindow*);

    void Add(CloseWatcher*);
    void Remove(CloseWatcher*);
    bool HasActiveWatcher() const { return !watchers_.IsEmpty(); }
    bool HasConsumedFreeWatcher() const;

    void Trace(Visitor*) const;

    void DidReceiveUserActivation() {
      user_activation_time_ = base::TimeTicks::Now();
    }
    void ConsumeCloseWatcherCancelability() {
      last_used_user_activation_time_ = user_activation_time_;
    }
    bool CanCloseWatcherFireCancel() const {
      return last_used_user_activation_time_ != user_activation_time_;
    }

    void EscapeKeyHandler(KeyboardEvent*);

   private:
    // mojom::blink::CloseListener override:
    void Signal() final;

    HeapLinkedHashSet<Member<CloseWatcher>> watchers_;

    // Holds a pipe which the service uses to notify this object
    // when the idle state has changed.
    HeapMojoReceiver<mojom::blink::CloseListener, WatcherStack> receiver_;

    // Tracks last use activation time consumed by a close watcher vs. last
    // time user activation happened:
    // https://wicg.github.io/close-watcher/#timestamp-of-last-activation-used-for-close-watchers
    base::TimeTicks user_activation_time_;
    base::TimeTicks last_used_user_activation_time_;

    Member<LocalDOMWindow> window_;
  };

 private:
  static CloseWatcher* CreateInternal(LocalDOMWindow*,
                                      WatcherStack&,
                                      CloseWatcherOptions*);

  enum class State { kActive, kClosed };
  State state_ = State::kActive;
  bool dispatching_cancel_ = false;
  bool grouped_with_previous_ = false;
  bool created_with_user_activation_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CLOSEWATCHER_CLOSE_WATCHER_H_
