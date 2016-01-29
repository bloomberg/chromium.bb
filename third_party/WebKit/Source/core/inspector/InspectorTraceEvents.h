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

namespace InspectorLayoutEvent {
PassRefPtr<TracedValue> beginData(FrameView*);
PassRefPtr<TracedValue> endData(LayoutObject* rootForThisLayout);
}

namespace InspectorScheduleStyleInvalidationTrackingEvent {
extern const char Attribute[];
extern const char Class[];
extern const char Id[];
extern const char Pseudo[];

PassRefPtr<TracedValue> attributeChange(Element&, const InvalidationSet&, const QualifiedName&);
PassRefPtr<TracedValue> classChange(Element&, const InvalidationSet&, const AtomicString&);
PassRefPtr<TracedValue> idChange(Element&, const InvalidationSet&, const AtomicString&);
PassRefPtr<TracedValue> pseudoChange(Element&, const InvalidationSet&, CSSSelector::PseudoType);
} // namespace InspectorScheduleStyleInvalidationTrackingEvent

#define TRACE_SCHEDULE_STYLE_INVALIDATION(element, invalidationSet, changeType, ...) \
    TRACE_EVENT_INSTANT1( \
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
        "ScheduleStyleInvalidationTracking", \
        TRACE_EVENT_SCOPE_THREAD, \
        "data", \
        InspectorScheduleStyleInvalidationTrackingEvent::changeType((element), (invalidationSet), __VA_ARGS__));

namespace InspectorStyleRecalcInvalidationTrackingEvent {
PassRefPtr<TracedValue> data(Node*, const StyleChangeReasonForTracing&);
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

PassRefPtr<TracedValue> data(Element&, const char* reason);
PassRefPtr<TracedValue> selectorPart(Element&, const char* reason, const InvalidationSet&, const String&);
PassRefPtr<TracedValue> invalidationList(Element&, const Vector<RefPtr<InvalidationSet>>&);
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
PassRefPtr<TracedValue> CORE_EXPORT data(const LayoutObject*, LayoutInvalidationReasonForTracing);
}

namespace InspectorPaintInvalidationTrackingEvent {
PassRefPtr<TracedValue> data(const LayoutObject*, const LayoutObject& paintContainer);
}

namespace InspectorScrollInvalidationTrackingEvent {
PassRefPtr<TracedValue> data(const LayoutObject&);
}

namespace InspectorSendRequestEvent {
PassRefPtr<TracedValue> data(unsigned long identifier, LocalFrame*, const ResourceRequest&);
}

namespace InspectorReceiveResponseEvent {
PassRefPtr<TracedValue> data(unsigned long identifier, LocalFrame*, const ResourceResponse&);
}

namespace InspectorReceiveDataEvent {
PassRefPtr<TracedValue> data(unsigned long identifier, LocalFrame*, int encodedDataLength);
}

namespace InspectorResourceFinishEvent {
PassRefPtr<TracedValue> data(unsigned long identifier, double finishTime, bool didFail);
}

namespace InspectorTimerInstallEvent {
PassRefPtr<TracedValue> data(ExecutionContext*, int timerId, int timeout, bool singleShot);
}

namespace InspectorTimerRemoveEvent {
PassRefPtr<TracedValue> data(ExecutionContext*, int timerId);
}

namespace InspectorTimerFireEvent {
PassRefPtr<TracedValue> data(ExecutionContext*, int timerId);
}

namespace InspectorIdleCallbackRequestEvent {
PassRefPtr<TracedValue> data(ExecutionContext*, int id, double timeout);
}

namespace InspectorIdleCallbackCancelEvent {
PassRefPtr<TracedValue> data(ExecutionContext*, int id);
}

namespace InspectorIdleCallbackFireEvent {
PassRefPtr<TracedValue> data(ExecutionContext*, int id, double allottedMilliseconds, bool timedOut);
}

namespace InspectorAnimationFrameEvent {
PassRefPtr<TracedValue> data(ExecutionContext*, int callbackId);
}

namespace InspectorParseHtmlEvent {
PassRefPtr<TracedValue> beginData(Document*, unsigned startLine);
PassRefPtr<TracedValue> endData(unsigned endLine);
}

namespace InspectorParseAuthorStyleSheetEvent {
PassRefPtr<TracedValue> data(const CSSStyleSheetResource*);
}

namespace InspectorXhrReadyStateChangeEvent {
PassRefPtr<TracedValue> data(ExecutionContext*, XMLHttpRequest*);
}

namespace InspectorXhrLoadEvent {
PassRefPtr<TracedValue> data(ExecutionContext*, XMLHttpRequest*);
}

namespace InspectorLayerInvalidationTrackingEvent {
extern const char SquashingLayerGeometryWasUpdated[];
extern const char AddedToSquashingLayer[];
extern const char RemovedFromSquashingLayer[];
extern const char ReflectionLayerChanged[];
extern const char NewCompositedLayer[];

PassRefPtr<TracedValue> data(const PaintLayer*, const char* reason);
}

#define TRACE_LAYER_INVALIDATION(LAYER, REASON) \
    TRACE_EVENT_INSTANT1( \
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
        "LayerInvalidationTracking", \
        TRACE_EVENT_SCOPE_THREAD, \
        "data", \
        InspectorLayerInvalidationTrackingEvent::data((LAYER), (REASON)));

namespace InspectorPaintEvent {
PassRefPtr<TracedValue> data(LayoutObject*, const LayoutRect& clipRect, const GraphicsLayer*);
}

namespace InspectorPaintImageEvent {
PassRefPtr<TracedValue> data(const LayoutImage&);
PassRefPtr<TracedValue> data(const LayoutObject&, const StyleImage&);
PassRefPtr<TracedValue> data(const LayoutObject*, const ImageResource&);
}

namespace InspectorCommitLoadEvent {
PassRefPtr<TracedValue> data(LocalFrame*);
}

namespace InspectorMarkLoadEvent {
PassRefPtr<TracedValue> data(LocalFrame*);
}

namespace InspectorScrollLayerEvent {
PassRefPtr<TracedValue> data(LayoutObject*);
}

namespace InspectorUpdateLayerTreeEvent {
PassRefPtr<TracedValue> data(LocalFrame*);
}

namespace InspectorEvaluateScriptEvent {
PassRefPtr<TracedValue> data(LocalFrame*, const String& url, const WTF::TextPosition&);
}

namespace InspectorCompileScriptEvent {
PassRefPtr<TracedValue> data(const String& url, const WTF::TextPosition&);
}

namespace InspectorFunctionCallEvent {
PassRefPtr<TracedValue> data(ExecutionContext*, int scriptId, const String& scriptName, int scriptLine);
}

namespace InspectorUpdateCountersEvent {
PassRefPtr<TracedValue> data();
}

namespace InspectorInvalidateLayoutEvent {
PassRefPtr<TracedValue> data(LocalFrame*);
}

namespace InspectorRecalculateStylesEvent {
PassRefPtr<TracedValue> data(LocalFrame*);
}

namespace InspectorEventDispatchEvent {
PassRefPtr<TracedValue> data(const Event&);
}

namespace InspectorTimeStampEvent {
PassRefPtr<TracedValue> data(ExecutionContext*, const String& message);
}

namespace InspectorTracingSessionIdForWorkerEvent {
PassRefPtr<TracedValue> data(const String& sessionId, const String& workerId, WorkerThread*);
}

namespace InspectorTracingStartedInFrame {
PassRefPtr<TracedValue> data(const String& sessionId, LocalFrame*);
}

namespace InspectorSetLayerTreeId {
PassRefPtr<TracedValue> data(const String& sessionId, int layerTreeId);
}

namespace InspectorAnimationEvent {
PassRefPtr<TracedValue> data(const Animation&);
}

namespace InspectorAnimationStateEvent {
PassRefPtr<TracedValue> data(const Animation&);
}

namespace InspectorHitTestEvent {
PassRefPtr<TracedValue> endData(const HitTestRequest&, const HitTestLocation&, const HitTestResult&);
}

CORE_EXPORT String toHexString(const void* p);
CORE_EXPORT void setCallStack(TracedValue*);

} // namespace blink


#endif // !defined(InspectorTraceEvents_h)
