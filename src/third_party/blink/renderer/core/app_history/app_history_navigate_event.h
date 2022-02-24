// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_NAVIGATE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_NAVIGATE_EVENT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_focus_reset.h"
#include "third_party/blink/renderer/core/app_history/app_history.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class AbortSignal;
class AppHistoryDestination;
class AppHistoryNavigateEventInit;
class AppHistoryTransitionWhileOptions;
class ExceptionState;
class FormData;
class ScriptPromise;

class AppHistoryNavigateEvent final : public Event,
                                      public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AppHistoryNavigateEvent* Create(ExecutionContext* context,
                                         const AtomicString& type,
                                         AppHistoryNavigateEventInit* init) {
    return MakeGarbageCollected<AppHistoryNavigateEvent>(context, type, init);
  }

  AppHistoryNavigateEvent(ExecutionContext* context,
                          const AtomicString& type,
                          AppHistoryNavigateEventInit* init);

  void SetUrl(const KURL& url) { url_ = url; }

  String navigationType() { return navigation_type_; }
  AppHistoryDestination* destination() { return destination_; }
  bool canTransition() const { return can_transition_; }
  bool userInitiated() const { return user_initiated_; }
  bool hashChange() const { return hash_change_; }
  AbortSignal* signal() { return signal_; }
  FormData* formData() const { return form_data_; }
  ScriptValue info() const { return info_; }

  void transitionWhile(ScriptState*,
                       ScriptPromise newNavigationAction,
                       AppHistoryTransitionWhileOptions*,
                       ExceptionState&);

  const HeapVector<ScriptPromise>& GetNavigationActionPromisesList() {
    return navigation_action_promises_list_;
  }
  bool ShouldResetFocus() const;

  const AtomicString& InterfaceName() const final;
  void Trace(Visitor*) const final;

 private:
  String navigation_type_;
  Member<AppHistoryDestination> destination_;
  bool can_transition_;
  bool user_initiated_;
  bool hash_change_;
  Member<AbortSignal> signal_;
  Member<FormData> form_data_;
  ScriptValue info_;
  absl::optional<V8AppHistoryFocusReset> focus_reset_behavior_ = absl::nullopt;

  KURL url_;
  HeapVector<ScriptPromise> navigation_action_promises_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_NAVIGATE_EVENT_H_
