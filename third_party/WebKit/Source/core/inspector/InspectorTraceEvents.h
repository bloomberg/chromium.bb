// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorTraceEvents_h
#define InspectorTraceEvents_h

#include "core/CoreExport.h"
#include "core/css/CSSSelector.h"
#include "platform/EventTracer.h"
#include "platform/TraceEvent.h"
#include "platform/TracedValue.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/Functional.h"

namespace v8 {
class Function;
template<typename T> class Local;
}

namespace WTF {
class TextPosition;
}

namespace blink {
class Animation;
class CSSStyleSheetResource;
class InvalidationSet;
class Document;
class Element;
class Event;
class ExecutionContext;
class FrameView;
class GraphicsLayer;
class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class ImageResource;
class KURL;
class PaintLayer;
class LayoutRect;
class LocalFrame;
class Node;
class QualifiedName;
class Page;
class LayoutImage;
class LayoutObject;
class ResourceRequest;
class ResourceResponse;
class StyleChangeReasonForTracing;
class StyleImage;
class WorkerThread;
class XMLHttpRequest;

enum ResourceLoadPriority : int;

namespace InspectorLayoutEvent {
PassOwnPtr<TracedValue> beginData(FrameView*);
PassOwnPtr<TracedValue> endData(LayoutObject* rootForThisLayout);
}

namespace InspectorScheduleStyleInvalidationTrackingEvent {
extern const char Attribute[];
extern const char Class[];
extern const char Id[];
extern const char Pseudo[];

PassOwnPtr<TracedValue> attributeChange(Element&, const InvalidationSet&, const QualifiedName&);
PassOwnPtr<TracedValue> classChange(Element&, const InvalidationSet&, const AtomicString&);
PassOwnPtr<TracedValue> idChange(Element&, const InvalidationSet&, const AtomicString&);
PassOwnPtr<TracedValue> pseudoChange(Element&, const InvalidationSet&, CSSSelector::PseudoType);
} // namespace InspectorScheduleStyleInvalidationTrackingEvent

#define TRACE_SCHEDULE_STYLE_INVALIDATION(element, invalidationSet, changeType, ...) \
    TRACE_EVENT_INSTANT1( \
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
        "ScheduleStyleInvalidationTracking", \
        TRACE_EVENT_SCOPE_THREAD, \
        "data", \
        InspectorScheduleStyleInvalidationTrackingEvent::changeType((element), (invalidationSet), __VA_ARGS__));

namespace InspectorStyleRecalcInvalidationTrackingEvent {
PassOwnPtr<TracedValue> data(Node*, const StyleChangeReasonForTracing&);
}

String descendantInvalidationSetToIdString(const InvalidationSet&);

namespace InspectorStyleInvalidatorInvalidateEvent {
extern const char ElementHasPendingInvalidationList[];
extern const char InvalidateCustomPseudo[];
extern const char InvalidationSetMatchedAttribute[];
extern const char InvalidationSetMatchedClass[];
extern const char InvalidationSetMatchedId[];
extern const char InvalidationSetMatchedTagName[];
extern const char PreventStyleSharingForParent[];

PassOwnPtr<TracedValue> data(Element&, const char* reason);
PassOwnPtr<TracedValue> selectorPart(Element&, const char* reason, const InvalidationSet&, const String&);
PassOwnPtr<TracedValue> invalidationList(Element&, const Vector<RefPtr<InvalidationSet>>&);
} // namespace InspectorStyleInvalidatorInvalidateEvent

#define TRACE_STYLE_INVALIDATOR_INVALIDATION(element, reason) \
    TRACE_EVENT_INSTANT1( \
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
        "StyleInvalidatorInvalidationTracking", \
        TRACE_EVENT_SCOPE_THREAD, \
        "data", \
        InspectorStyleInvalidatorInvalidateEvent::data((element), (InspectorStyleInvalidatorInvalidateEvent::reason)))

#define TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART(element, reason, invalidationSet, singleSelectorPart) \
    TRACE_EVENT_INSTANT1( \
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
        "StyleInvalidatorInvalidationTracking", \
        TRACE_EVENT_SCOPE_THREAD, \
        "data", \
        InspectorStyleInvalidatorInvalidateEvent::selectorPart((element), (InspectorStyleInvalidatorInvalidateEvent::reason), (invalidationSet), (singleSelectorPart)))

// From a web developer's perspective: what caused this layout? This is strictly
// for tracing. Blink logic must not depend on these.
namespace LayoutInvalidationReason {
extern const char Unknown[];
extern const char SizeChanged[];
extern const char AncestorMoved[];
extern const char StyleChange[];
extern const char DomChanged[];
extern const char TextChanged[];
extern const char PrintingChanged[];
extern const char AttributeChanged[];
extern const char ColumnsChanged[];
extern const char ChildAnonymousBlockChanged[];
extern const char AnonymousBlockChange[];
extern const char Fullscreen[];
extern const char ChildChanged[];
extern const char ListValueChange[];
extern const char ImageChanged[];
extern const char LineBoxesChanged[];
extern const char SliderValueChanged[];
extern const char AncestorMarginCollapsing[];
extern const char FieldsetChanged[];
extern const char TextAutosizing[];
extern const char SvgResourceInvalidated[];
extern const char FloatDescendantChanged[];
extern const char CountersChanged[];
extern const char GridChanged[];
extern const char MenuWidthChanged[];
extern const char RemovedFromLayout[];
extern const char AddedToLayout[];
extern const char TableChanged[];
extern const char PaddingChanged[];
extern const char TextControlChanged[];
// FIXME: This is too generic, we should be able to split out transform and
// size related invalidations.
extern const char SvgChanged[];
extern const char ScrollbarChanged[];
} // namespace LayoutInvalidationReason

// LayoutInvalidationReasonForTracing is strictly for tracing. Blink logic must
// not depend on this value.
typedef const char LayoutInvalidationReasonForTracing[];

namespace InspectorLayoutInvalidationTrackingEvent {
PassOwnPtr<TracedValue> CORE_EXPORT data(const LayoutObject*, LayoutInvalidationReasonForTracing);
}

namespace InspectorPaintInvalidationTrackingEvent {
PassOwnPtr<TracedValue> data(const LayoutObject*, const LayoutObject& paintContainer);
}

namespace InspectorScrollInvalidationTrackingEvent {
PassOwnPtr<TracedValue> data(const LayoutObject&);
}

namespace InspectorChangeResourcePriorityEvent {
PassOwnPtr<TracedValue> data(unsigned long identifier, const ResourceLoadPriority&);
}

namespace InspectorSendRequestEvent {
PassOwnPtr<TracedValue> data(unsigned long identifier, LocalFrame*, const ResourceRequest&);
}

namespace InspectorReceiveResponseEvent {
PassOwnPtr<TracedValue> data(unsigned long identifier, LocalFrame*, const ResourceResponse&);
}

namespace InspectorReceiveDataEvent {
PassOwnPtr<TracedValue> data(unsigned long identifier, LocalFrame*, int encodedDataLength);
}

namespace InspectorResourceFinishEvent {
PassOwnPtr<TracedValue> data(unsigned long identifier, double finishTime, bool didFail);
}

namespace InspectorTimerInstallEvent {
PassOwnPtr<TracedValue> data(ExecutionContext*, int timerId, int timeout, bool singleShot);
}

namespace InspectorTimerRemoveEvent {
PassOwnPtr<TracedValue> data(ExecutionContext*, int timerId);
}

namespace InspectorTimerFireEvent {
PassOwnPtr<TracedValue> data(ExecutionContext*, int timerId);
}

namespace InspectorIdleCallbackRequestEvent {
PassOwnPtr<TracedValue> data(ExecutionContext*, int id, double timeout);
}

namespace InspectorIdleCallbackCancelEvent {
PassOwnPtr<TracedValue> data(ExecutionContext*, int id);
}

namespace InspectorIdleCallbackFireEvent {
PassOwnPtr<TracedValue> data(ExecutionContext*, int id, double allottedMilliseconds, bool timedOut);
}

namespace InspectorAnimationFrameEvent {
PassOwnPtr<TracedValue> data(ExecutionContext*, int callbackId);
}

namespace InspectorParseHtmlEvent {
PassOwnPtr<TracedValue> beginData(Document*, unsigned startLine);
PassOwnPtr<TracedValue> endData(unsigned endLine);
}

namespace InspectorParseAuthorStyleSheetEvent {
PassOwnPtr<TracedValue> data(const CSSStyleSheetResource*);
}

namespace InspectorXhrReadyStateChangeEvent {
PassOwnPtr<TracedValue> data(ExecutionContext*, XMLHttpRequest*);
}

namespace InspectorXhrLoadEvent {
PassOwnPtr<TracedValue> data(ExecutionContext*, XMLHttpRequest*);
}

namespace InspectorLayerInvalidationTrackingEvent {
extern const char SquashingLayerGeometryWasUpdated[];
extern const char AddedToSquashingLayer[];
extern const char RemovedFromSquashingLayer[];
extern const char ReflectionLayerChanged[];
extern const char NewCompositedLayer[];

PassOwnPtr<TracedValue> data(const PaintLayer*, const char* reason);
}

#define TRACE_LAYER_INVALIDATION(LAYER, REASON) \
    TRACE_EVENT_INSTANT1( \
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
        "LayerInvalidationTracking", \
        TRACE_EVENT_SCOPE_THREAD, \
        "data", \
        InspectorLayerInvalidationTrackingEvent::data((LAYER), (REASON)));

namespace InspectorPaintEvent {
PassOwnPtr<TracedValue> data(LayoutObject*, const LayoutRect& clipRect, const GraphicsLayer*);
}

namespace InspectorPaintImageEvent {
PassOwnPtr<TracedValue> data(const LayoutImage&);
PassOwnPtr<TracedValue> data(const LayoutObject&, const StyleImage&);
PassOwnPtr<TracedValue> data(const LayoutObject*, const ImageResource&);
}

namespace InspectorCommitLoadEvent {
PassOwnPtr<TracedValue> data(LocalFrame*);
}

namespace InspectorMarkLoadEvent {
PassOwnPtr<TracedValue> data(LocalFrame*);
}

namespace InspectorScrollLayerEvent {
PassOwnPtr<TracedValue> data(LayoutObject*);
}

namespace InspectorUpdateLayerTreeEvent {
PassOwnPtr<TracedValue> data(LocalFrame*);
}

namespace InspectorEvaluateScriptEvent {
PassOwnPtr<TracedValue> data(LocalFrame*, const String& url, const WTF::TextPosition&);
}

namespace InspectorParseScriptEvent {
PassOwnPtr<TracedValue> data(unsigned long identifier, const String& url);
}

namespace InspectorCompileScriptEvent {
PassOwnPtr<TracedValue> data(const String& url, const WTF::TextPosition&);
}

namespace InspectorFunctionCallEvent {
PassOwnPtr<TracedValue> data(ExecutionContext*, const v8::Local<v8::Function>&);
}

namespace InspectorUpdateCountersEvent {
PassOwnPtr<TracedValue> data();
}

namespace InspectorInvalidateLayoutEvent {
PassOwnPtr<TracedValue> data(LocalFrame*);
}

namespace InspectorRecalculateStylesEvent {
PassOwnPtr<TracedValue> data(LocalFrame*);
}

namespace InspectorEventDispatchEvent {
PassOwnPtr<TracedValue> data(const Event&);
}

namespace InspectorTimeStampEvent {
PassOwnPtr<TracedValue> data(ExecutionContext*, const String& message);
}

namespace InspectorTracingSessionIdForWorkerEvent {
PassOwnPtr<TracedValue> data(const String& sessionId, const String& workerId, WorkerThread*);
}

namespace InspectorTracingStartedInFrame {
PassOwnPtr<TracedValue> data(const String& sessionId, LocalFrame*);
}

namespace InspectorSetLayerTreeId {
PassOwnPtr<TracedValue> data(const String& sessionId, int layerTreeId);
}

namespace InspectorAnimationEvent {
PassOwnPtr<TracedValue> data(const Animation&);
}

namespace InspectorAnimationStateEvent {
PassOwnPtr<TracedValue> data(const Animation&);
}

namespace InspectorHitTestEvent {
PassOwnPtr<TracedValue> endData(const HitTestRequest&, const HitTestLocation&, const HitTestResult&);
}

CORE_EXPORT String toHexString(const void* p);
CORE_EXPORT void setCallStack(TracedValue*);

} // namespace blink


#endif // !defined(InspectorTraceEvents_h)
