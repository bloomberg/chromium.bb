// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_API_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_API_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/navigation/navigation_api_history_entry_arrays.mojom-blink.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class NavigationApiNavigation;
class NavigationUpdateCurrentEntryOptions;
class NavigationHistoryEntry;
class NavigateEvent;
class NavigationNavigateOptions;
class NavigationReloadOptions;
class NavigationResult;
class NavigationOptions;
class NavigationTransition;
class DOMException;
class HTMLFormElement;
class HistoryItem;
class KURL;
class SerializedScriptValue;

// TODO(japhet): This should probably move to frame_loader_types.h and possibly
// be used more broadly once it is in the HTML spec.
enum class UserNavigationInvolvement { kBrowserUI, kActivation, kNone };
enum class NavigateEventType { kFragment, kHistoryApi, kCrossDocument };

class CORE_EXPORT NavigationApi final
    : public EventTargetWithInlineData,
      public Supplement<LocalDOMWindow>,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static NavigationApi* navigation(LocalDOMWindow&);
  // Unconditionally creates NavigationApi, even if the RuntimeEnabledFeatures
  // is disabled.
  static NavigationApi* From(LocalDOMWindow&);
  explicit NavigationApi(LocalDOMWindow&);
  ~NavigationApi() final = default;

  void InitializeForNewWindow(HistoryItem& current,
                              WebFrameLoadType,
                              CommitReason,
                              NavigationApi* previous,
                              const WebVector<WebHistoryItem>& back_entries,
                              const WebVector<WebHistoryItem>& forward_entries);
  void UpdateForNavigation(HistoryItem&, WebFrameLoadType);
  void SetEntriesForRestore(
      const mojom::blink::NavigationApiHistoryEntryArraysPtr&);

  // From the navigation API's perspective, a dropped navigation is still
  // "ongoing"; that is, ongoing_navigation_event_ and ongoing_navigation_ are
  // non-null. (The impact of this is that another navigation will cancel that
  // ongoing navigation, in a web-developer-visible way.) But from the
  // perspective of other parts of Chromium which interface with the navigation
  // API, e.g. to deal with the loading spinner, dropped navigations are not
  // what they care about.
  bool HasNonDroppedOngoingNavigation() const;

  // Web-exposed:
  NavigationHistoryEntry* currentEntry() const;
  HeapVector<Member<NavigationHistoryEntry>> entries();
  void updateCurrentEntry(NavigationUpdateCurrentEntryOptions*,
                          ExceptionState&);
  NavigationTransition* transition() const { return transition_; }

  bool canGoBack() const;
  bool canGoForward() const;

  NavigationResult* navigate(ScriptState*,
                             const String& url,
                             NavigationNavigateOptions*);
  NavigationResult* reload(ScriptState*, NavigationReloadOptions*);

  NavigationResult* traverseTo(ScriptState*,
                               const String& key,
                               NavigationOptions*);
  NavigationResult* back(ScriptState*, NavigationOptions*);
  NavigationResult* forward(ScriptState*, NavigationOptions*);

  // onnavigate is defined manually so that a UseCounter can be applied to just
  // the setter
  void setOnnavigate(EventListener* listener);
  EventListener* onnavigate() {
    return GetAttributeEventListener(event_type_names::kNavigate);
  }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(navigatesuccess, kNavigatesuccess)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(navigateerror, kNavigateerror)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(currententrychange, kCurrententrychange)

  enum class DispatchResult { kContinue, kAbort, kTransitionWhile };
  struct DispatchParams {
    STACK_ALLOCATED();

   public:
    DispatchParams(const KURL& url_in,
                   NavigateEventType event_type_in,
                   WebFrameLoadType frame_load_type_in)
        : url(url_in),
          event_type(event_type_in),
          frame_load_type(frame_load_type_in) {}

    const KURL url;
    const NavigateEventType event_type;
    const WebFrameLoadType frame_load_type;
    UserNavigationInvolvement involvement = UserNavigationInvolvement::kNone;
    HTMLFormElement* form = nullptr;
    SerializedScriptValue* state_object = nullptr;
    HistoryItem* destination_item = nullptr;
    bool is_browser_initiated = false;
    bool is_synchronously_committed_same_document = true;
    String download_filename;
  };
  DispatchResult DispatchNavigateEvent(const DispatchParams&);

  // In the spec, we are only informed about canceled navigations. But in the
  // implementation we need to handle other cases:
  // - "Dropped" navigations, e.g. navigations to 204/205/Content-Disposition:
  //   attachment, need to be tracked for |HasNonDroppedOngoingNavigation()|.
  //   (See https://github.com/WICG/navigation-api/issues/137 for more on why
  //   they must be ignored.) This distinction is handled via the |reason|
  //   argument.
  // - If the frame is destroyed without canceling ongoing navigations, e.g. due
  //   to a cross-document navigation, then we need to detach any outstanding
  //   promise resolvers. This is handled via |ContextDestroyed()| below.
  void InformAboutCanceledNavigation(
      CancelNavigationReason reason = CancelNavigationReason::kOther);

  int GetIndexFor(NavigationHistoryEntry*);

  // EventTargetWithInlineData overrides:
  const AtomicString& InterfaceName() const final;
  ExecutionContext* GetExecutionContext() const final {
    return GetSupplementable();
  }

  void Trace(Visitor*) const final;

 protected:
  // ExecutionContextLifecycleObserver implementation:
  void ContextDestroyed() override;

 private:
  friend class NavigateReaction;
  friend class NavigationApiNavigation;
  NavigationHistoryEntry* GetEntryForRestore(
      const mojom::blink::NavigationApiHistoryEntryPtr&);
  void PopulateKeySet();
  void FinalizeWithAbortedNavigationError(ScriptState*,
                                          NavigationApiNavigation*);
  void ResolvePromisesAndFireNavigateSuccessEvent(NavigationApiNavigation*);
  void RejectPromisesAndFireNavigateErrorEvent(NavigationApiNavigation*,
                                               ScriptValue);

  NavigationResult* PerformNonTraverseNavigation(
      ScriptState*,
      FrameLoadRequest&,
      scoped_refptr<SerializedScriptValue>,
      NavigationOptions*,
      WebFrameLoadType);

  DOMException* PerformSharedNavigationChecks(
      const String& method_name_for_error_message);

  void PromoteUpcomingNavigationToOngoing(const String& key);
  void CleanupApiNavigation(NavigationApiNavigation&);

  scoped_refptr<SerializedScriptValue> SerializeState(const ScriptValue&,
                                                      ExceptionState&);

  bool HasEntriesAndEventsDisabled() const;

  NavigationHistoryEntry* MakeEntryFromItem(HistoryItem&);

  HeapVector<Member<NavigationHistoryEntry>> entries_;
  HashMap<String, int> keys_to_indices_;
  int current_entry_index_ = -1;
  bool has_dropped_navigation_ = false;

  Member<NavigationTransition> transition_;

  Member<NavigationApiNavigation> ongoing_navigation_;
  HeapHashMap<String, Member<NavigationApiNavigation>> upcoming_traversals_;
  Member<NavigationApiNavigation> upcoming_non_traversal_navigation_;

  Member<NavigateEvent> ongoing_navigate_event_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_API_H_
