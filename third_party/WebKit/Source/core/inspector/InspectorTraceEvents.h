// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorTraceEvents_h
#define InspectorTraceEvents_h

#include <memory>

#include "core/CoreExport.h"
#include "core/css/CSSSelector.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Functional.h"

namespace v8 {
class Function;
template <typename T>
class Local;
}

namespace WTF {
class TextPosition;
}

namespace blink {
class Animation;
class CSSStyleSheetResource;
class ContainerNode;
class DocumentLoader;
class Element;
class Event;
class ExecutionContext;
struct FetchInitiatorInfo;
class FloatRect;
class GraphicsLayer;
class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class ImageResourceContent;
class InvalidationSet;
class LayoutImage;
class LayoutObject;
class LayoutRect;
class LocalFrame;
class LocalFrameView;
class Node;
class PaintLayer;
class QualifiedName;
class Resource;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class StyleChangeReasonForTracing;
class StyleImage;
class WorkerThread;
class XMLHttpRequest;

namespace probe {
class CallFunction;
class ExecuteScript;
class ParseHTML;
}

class CORE_EXPORT InspectorTraceEvents : public InspectorAgent {
  WTF_MAKE_NONCOPYABLE(InspectorTraceEvents);

 public:
  InspectorTraceEvents() {}

  void Init(CoreProbeSink*,
            protocol::UberDispatcher*,
            protocol::DictionaryValue*) override;
  void Dispose() override;
  void WillSendRequest(ExecutionContext*,
                       unsigned long identifier,
                       DocumentLoader*,
                       ResourceRequest&,
                       const ResourceResponse& redirect_response,
                       const FetchInitiatorInfo&,
                       Resource::Type);
  void DidReceiveResourceResponse(unsigned long identifier,
                                  DocumentLoader*,
                                  const ResourceResponse&,
                                  Resource*);
  void DidReceiveData(unsigned long identifier,
                      DocumentLoader*,
                      const char* data,
                      int data_length);
  void DidFinishLoading(unsigned long identifier,
                        DocumentLoader*,
                        double monotonic_finish_time,
                        int64_t encoded_data_length,
                        int64_t decoded_body_length);
  void DidFailLoading(unsigned long identifier,
                      DocumentLoader*,
                      const ResourceError&);

  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);

  void Will(const probe::ParseHTML&);
  void Did(const probe::ParseHTML&);

  void Will(const probe::CallFunction&);
  void Did(const probe::CallFunction&);

  void PaintTiming(Document*, const char* name, double timestamp);

  virtual void Trace(blink::Visitor*);

 private:
  Member<CoreProbeSink> instrumenting_agents_;
};

namespace InspectorLayoutEvent {
std::unique_ptr<TracedValue> BeginData(LocalFrameView*);
std::unique_ptr<TracedValue> EndData(LayoutObject* root_for_this_layout);
}

namespace InspectorScheduleStyleInvalidationTrackingEvent {
extern const char kAttribute[];
extern const char kClass[];
extern const char kId[];
extern const char kPseudo[];
extern const char kRuleSet[];

std::unique_ptr<TracedValue> AttributeChange(Element&,
                                             const InvalidationSet&,
                                             const QualifiedName&);
std::unique_ptr<TracedValue> ClassChange(Element&,
                                         const InvalidationSet&,
                                         const AtomicString&);
std::unique_ptr<TracedValue> IdChange(Element&,
                                      const InvalidationSet&,
                                      const AtomicString&);
std::unique_ptr<TracedValue> PseudoChange(Element&,
                                          const InvalidationSet&,
                                          CSSSelector::PseudoType);
std::unique_ptr<TracedValue> RuleSetInvalidation(ContainerNode&,
                                                 const InvalidationSet&);
}  // namespace InspectorScheduleStyleInvalidationTrackingEvent

#define TRACE_SCHEDULE_STYLE_INVALIDATION(element, invalidationSet,          \
                                          changeType, ...)                   \
  TRACE_EVENT_INSTANT1(                                                      \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),   \
      "ScheduleStyleInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data", \
      InspectorScheduleStyleInvalidationTrackingEvent::changeType(           \
          (element), (invalidationSet), ##__VA_ARGS__));

namespace InspectorStyleRecalcInvalidationTrackingEvent {
std::unique_ptr<TracedValue> Data(Node*, const StyleChangeReasonForTracing&);
}

String DescendantInvalidationSetToIdString(const InvalidationSet&);

namespace InspectorStyleInvalidatorInvalidateEvent {
extern const char kElementHasPendingInvalidationList[];
extern const char kInvalidateCustomPseudo[];
extern const char kInvalidationSetMatchedAttribute[];
extern const char kInvalidationSetMatchedClass[];
extern const char kInvalidationSetMatchedId[];
extern const char kInvalidationSetMatchedTagName[];

std::unique_ptr<TracedValue> Data(Element&, const char* reason);
std::unique_ptr<TracedValue> SelectorPart(Element&,
                                          const char* reason,
                                          const InvalidationSet&,
                                          const String&);
std::unique_ptr<TracedValue> InvalidationList(
    ContainerNode&,
    const Vector<RefPtr<InvalidationSet>>&);
}  // namespace InspectorStyleInvalidatorInvalidateEvent

#define TRACE_STYLE_INVALIDATOR_INVALIDATION(element, reason)              \
  TRACE_EVENT_INSTANT1(                                                    \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
      "StyleInvalidatorInvalidationTracking", TRACE_EVENT_SCOPE_THREAD,    \
      "data",                                                              \
      InspectorStyleInvalidatorInvalidateEvent::Data(                      \
          (element), (InspectorStyleInvalidatorInvalidateEvent::reason)))

#define TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART(                 \
    element, reason, invalidationSet, singleSelectorPart)                  \
  TRACE_EVENT_INSTANT1(                                                    \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
      "StyleInvalidatorInvalidationTracking", TRACE_EVENT_SCOPE_THREAD,    \
      "data",                                                              \
      InspectorStyleInvalidatorInvalidateEvent::SelectorPart(              \
          (element), (InspectorStyleInvalidatorInvalidateEvent::reason),   \
          (invalidationSet), (singleSelectorPart)))

// From a web developer's perspective: what caused this layout? This is strictly
// for tracing. Blink logic must not depend on these.
namespace LayoutInvalidationReason {
extern const char kUnknown[];
extern const char kSizeChanged[];
extern const char kAncestorMoved[];
extern const char kStyleChange[];
extern const char kDomChanged[];
extern const char kTextChanged[];
extern const char kPrintingChanged[];
extern const char kAttributeChanged[];
extern const char kColumnsChanged[];
extern const char kChildAnonymousBlockChanged[];
extern const char kAnonymousBlockChange[];
extern const char kFullscreen[];
extern const char kChildChanged[];
extern const char kListValueChange[];
extern const char kImageChanged[];
extern const char kLineBoxesChanged[];
extern const char kSliderValueChanged[];
extern const char kAncestorMarginCollapsing[];
extern const char kFieldsetChanged[];
extern const char kTextAutosizing[];
extern const char kSvgResourceInvalidated[];
extern const char kFloatDescendantChanged[];
extern const char kCountersChanged[];
extern const char kGridChanged[];
extern const char kMenuOptionsChanged[];
extern const char kRemovedFromLayout[];
extern const char kAddedToLayout[];
extern const char kTableChanged[];
extern const char kPaddingChanged[];
extern const char kTextControlChanged[];
// FIXME: This is too generic, we should be able to split out transform and
// size related invalidations.
extern const char kSvgChanged[];
extern const char kScrollbarChanged[];
}  // namespace LayoutInvalidationReason

// LayoutInvalidationReasonForTracing is strictly for tracing. Blink logic must
// not depend on this value.
typedef const char LayoutInvalidationReasonForTracing[];

namespace InspectorLayoutInvalidationTrackingEvent {
std::unique_ptr<TracedValue> CORE_EXPORT
Data(const LayoutObject*, LayoutInvalidationReasonForTracing);
}

namespace InspectorPaintInvalidationTrackingEvent {
std::unique_ptr<TracedValue> Data(const LayoutObject*,
                                  const LayoutObject& paint_container);
}

namespace InspectorScrollInvalidationTrackingEvent {
std::unique_ptr<TracedValue> Data(const LayoutObject&);
}

namespace InspectorChangeResourcePriorityEvent {
std::unique_ptr<TracedValue> Data(unsigned long identifier,
                                  const ResourceLoadPriority&);
}

namespace InspectorSendRequestEvent {
std::unique_ptr<TracedValue> Data(unsigned long identifier,
                                  LocalFrame*,
                                  const ResourceRequest&);
}

namespace InspectorReceiveResponseEvent {
std::unique_ptr<TracedValue> Data(unsigned long identifier,
                                  LocalFrame*,
                                  const ResourceResponse&);
}

namespace InspectorReceiveDataEvent {
std::unique_ptr<TracedValue> Data(unsigned long identifier,
                                  LocalFrame*,
                                  int encoded_data_length);
}

namespace InspectorResourceFinishEvent {
std::unique_ptr<TracedValue> Data(unsigned long identifier,
                                  double finish_time,
                                  bool did_fail,
                                  int64_t encoded_data_length,
                                  int64_t decoded_body_length);
}

namespace InspectorTimerInstallEvent {
std::unique_ptr<TracedValue> Data(ExecutionContext*,
                                  int timer_id,
                                  int timeout,
                                  bool single_shot);
}

namespace InspectorTimerRemoveEvent {
std::unique_ptr<TracedValue> Data(ExecutionContext*, int timer_id);
}

namespace InspectorTimerFireEvent {
std::unique_ptr<TracedValue> Data(ExecutionContext*, int timer_id);
}

namespace InspectorIdleCallbackRequestEvent {
std::unique_ptr<TracedValue> Data(ExecutionContext*, int id, double timeout);
}

namespace InspectorIdleCallbackCancelEvent {
std::unique_ptr<TracedValue> Data(ExecutionContext*, int id);
}

namespace InspectorIdleCallbackFireEvent {
std::unique_ptr<TracedValue> Data(ExecutionContext*,
                                  int id,
                                  double allotted_milliseconds,
                                  bool timed_out);
}

namespace InspectorAnimationFrameEvent {
std::unique_ptr<TracedValue> Data(ExecutionContext*, int callback_id);
}

namespace InspectorParseAuthorStyleSheetEvent {
std::unique_ptr<TracedValue> Data(const CSSStyleSheetResource*);
}

namespace InspectorXhrReadyStateChangeEvent {
std::unique_ptr<TracedValue> Data(ExecutionContext*, XMLHttpRequest*);
}

namespace InspectorXhrLoadEvent {
std::unique_ptr<TracedValue> Data(ExecutionContext*, XMLHttpRequest*);
}

namespace InspectorLayerInvalidationTrackingEvent {
extern const char kSquashingLayerGeometryWasUpdated[];
extern const char kAddedToSquashingLayer[];
extern const char kRemovedFromSquashingLayer[];
extern const char kReflectionLayerChanged[];
extern const char kNewCompositedLayer[];

std::unique_ptr<TracedValue> Data(const PaintLayer*, const char* reason);
}

#define TRACE_LAYER_INVALIDATION(LAYER, REASON)                            \
  TRACE_EVENT_INSTANT1(                                                    \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
      "LayerInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data",       \
      InspectorLayerInvalidationTrackingEvent::Data((LAYER), (REASON)));

namespace InspectorPaintEvent {
std::unique_ptr<TracedValue> Data(LayoutObject*,
                                  const LayoutRect& clip_rect,
                                  const GraphicsLayer*);
}

namespace InspectorPaintImageEvent {
std::unique_ptr<TracedValue> Data(const LayoutImage&,
                                  const FloatRect& src_rect,
                                  const FloatRect& dest_rect);
std::unique_ptr<TracedValue> Data(const LayoutObject&, const StyleImage&);
std::unique_ptr<TracedValue> Data(Node*,
                                  const StyleImage&,
                                  const FloatRect& src_rect,
                                  const FloatRect& dest_rect);
std::unique_ptr<TracedValue> Data(const LayoutObject*,
                                  const ImageResourceContent&);
}

namespace InspectorCommitLoadEvent {
std::unique_ptr<TracedValue> Data(LocalFrame*);
}

namespace InspectorMarkLoadEvent {
std::unique_ptr<TracedValue> Data(LocalFrame*);
}

namespace InspectorScrollLayerEvent {
std::unique_ptr<TracedValue> Data(LayoutObject*);
}

namespace InspectorUpdateLayerTreeEvent {
std::unique_ptr<TracedValue> Data(LocalFrame*);
}

namespace InspectorEvaluateScriptEvent {
std::unique_ptr<TracedValue> Data(LocalFrame*,
                                  const String& url,
                                  const WTF::TextPosition&);
}

namespace InspectorParseScriptEvent {
std::unique_ptr<TracedValue> Data(unsigned long identifier, const String& url);
}

namespace InspectorCompileScriptEvent {
std::unique_ptr<TracedValue> Data(const String& url, const WTF::TextPosition&);
}

namespace InspectorFunctionCallEvent {
std::unique_ptr<TracedValue> Data(ExecutionContext*,
                                  const v8::Local<v8::Function>&);
}

namespace InspectorUpdateCountersEvent {
std::unique_ptr<TracedValue> Data();
}

namespace InspectorInvalidateLayoutEvent {
std::unique_ptr<TracedValue> Data(LocalFrame*);
}

namespace InspectorRecalculateStylesEvent {
std::unique_ptr<TracedValue> Data(LocalFrame*);
}

namespace InspectorEventDispatchEvent {
std::unique_ptr<TracedValue> Data(const Event&);
}

namespace InspectorTimeStampEvent {
std::unique_ptr<TracedValue> Data(ExecutionContext*, const String& message);
}

namespace InspectorTracingSessionIdForWorkerEvent {
std::unique_ptr<TracedValue> Data(const String& session_id,
                                  const String& worker_id,
                                  WorkerThread*);
}

namespace InspectorTracingStartedInFrame {
std::unique_ptr<TracedValue> Data(const String& session_id, LocalFrame*);
}

namespace InspectorSetLayerTreeId {
std::unique_ptr<TracedValue> Data(const String& session_id, int layer_tree_id);
}

namespace InspectorAnimationEvent {
std::unique_ptr<TracedValue> Data(const Animation&);
}

namespace InspectorAnimationStateEvent {
std::unique_ptr<TracedValue> Data(const Animation&);
}

namespace InspectorHitTestEvent {
std::unique_ptr<TracedValue> EndData(const HitTestRequest&,
                                     const HitTestLocation&,
                                     const HitTestResult&);
}

namespace InspectorAsyncTask {
std::unique_ptr<TracedValue> Data(const String&);
}

CORE_EXPORT String ToHexString(const void* p);
CORE_EXPORT void SetCallStack(TracedValue*);

}  // namespace blink

#endif  // !defined(InspectorTraceEvents_h)
