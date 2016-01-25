// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorTraceEvents_h
#define InspectorTraceEvents_h

#include "core/CoreExport.h"
#include "core/css/CSSSelector.h"
#include "platform/EventTracer.h"
#include "platform/TraceEvent.h"
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
class TracedValue;
class WorkerThread;
class XMLHttpRequest;

namespace InspectorLayoutEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> beginData(FrameView*);
PassRefPtr<TraceEvent::ConvertableToTraceFormat> endData(LayoutObject* rootForThisLayout);
}

namespace InspectorScheduleStyleInvalidationTrackingEvent {
extern const char Attribute[];
extern const char Class[];
extern const char Id[];
extern const char Pseudo[];

PassRefPtr<TraceEvent::ConvertableToTraceFormat> attributeChange(Element&, const InvalidationSet&, const QualifiedName&);
PassRefPtr<TraceEvent::ConvertableToTraceFormat> classChange(Element&, const InvalidationSet&, const AtomicString&);
PassRefPtr<TraceEvent::ConvertableToTraceFormat> idChange(Element&, const InvalidationSet&, const AtomicString&);
PassRefPtr<TraceEvent::ConvertableToTraceFormat> pseudoChange(Element&, const InvalidationSet&, CSSSelector::PseudoType);
} // namespace InspectorScheduleStyleInvalidationTrackingEvent

#define TRACE_SCHEDULE_STYLE_INVALIDATION(element, invalidationSet, changeType, ...) \
    TRACE_EVENT_INSTANT1( \
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
        "ScheduleStyleInvalidationTracking", \
        TRACE_EVENT_SCOPE_THREAD, \
        "data", \
        InspectorScheduleStyleInvalidationTrackingEvent::changeType((element), (invalidationSet), __VA_ARGS__));

namespace InspectorStyleRecalcInvalidationTrackingEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(Node*, const StyleChangeReasonForTracing&);
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

PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(Element&, const char* reason);
PassRefPtr<TraceEvent::ConvertableToTraceFormat> selectorPart(Element&, const char* reason, const InvalidationSet&, const String&);
PassRefPtr<TraceEvent::ConvertableToTraceFormat> invalidationList(Element&, const Vector<RefPtr<InvalidationSet>>&);
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
PassRefPtr<TraceEvent::ConvertableToTraceFormat> CORE_EXPORT data(const LayoutObject*, LayoutInvalidationReasonForTracing);
}

namespace InspectorPaintInvalidationTrackingEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const LayoutObject*, const LayoutObject& paintContainer);
}

namespace InspectorScrollInvalidationTrackingEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const LayoutObject&);
}

namespace InspectorSendRequestEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, LocalFrame*, const ResourceRequest&);
}

namespace InspectorReceiveResponseEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, LocalFrame*, const ResourceResponse&);
}

namespace InspectorReceiveDataEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, LocalFrame*, int encodedDataLength);
}

namespace InspectorResourceFinishEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, double finishTime, bool didFail);
}

namespace InspectorTimerInstallEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, int timerId, int timeout, bool singleShot);
}

namespace InspectorTimerRemoveEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, int timerId);
}

namespace InspectorTimerFireEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, int timerId);
}

namespace InspectorIdleCallbackRequestEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, int id, double timeout);
}

namespace InspectorIdleCallbackCancelEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, int id);
}

namespace InspectorIdleCallbackFireEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, int id, double allottedMilliseconds, bool timedOut);
}

namespace InspectorAnimationFrameEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, int callbackId);
}

namespace InspectorParseHtmlEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> beginData(Document*, unsigned startLine);
PassRefPtr<TraceEvent::ConvertableToTraceFormat> endData(unsigned endLine);
}

namespace InspectorParseAuthorStyleSheetEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const CSSStyleSheetResource*);
}

namespace InspectorXhrReadyStateChangeEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, XMLHttpRequest*);
}

namespace InspectorXhrLoadEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, XMLHttpRequest*);
}

namespace InspectorLayerInvalidationTrackingEvent {
extern const char SquashingLayerGeometryWasUpdated[];
extern const char AddedToSquashingLayer[];
extern const char RemovedFromSquashingLayer[];
extern const char ReflectionLayerChanged[];
extern const char NewCompositedLayer[];

PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const PaintLayer*, const char* reason);
}

#define TRACE_LAYER_INVALIDATION(LAYER, REASON) \
    TRACE_EVENT_INSTANT1( \
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
        "LayerInvalidationTracking", \
        TRACE_EVENT_SCOPE_THREAD, \
        "data", \
        InspectorLayerInvalidationTrackingEvent::data((LAYER), (REASON)));

namespace InspectorPaintEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(LayoutObject*, const LayoutRect& clipRect, const GraphicsLayer*);
}

namespace InspectorPaintImageEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const LayoutImage&);
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const LayoutObject&, const StyleImage&);
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const LayoutObject*, const ImageResource&);
}

namespace InspectorCommitLoadEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(LocalFrame*);
}

namespace InspectorMarkLoadEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(LocalFrame*);
}

namespace InspectorScrollLayerEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(LayoutObject*);
}

namespace InspectorUpdateLayerTreeEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(LocalFrame*);
}

namespace InspectorEvaluateScriptEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(LocalFrame*, const String& url, const WTF::TextPosition&);
}

namespace InspectorCompileScriptEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const String& url, const WTF::TextPosition&);
}

namespace InspectorFunctionCallEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, int scriptId, const String& scriptName, int scriptLine);
}

namespace InspectorUpdateCountersEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data();
}

namespace InspectorInvalidateLayoutEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(LocalFrame*);
}

namespace InspectorRecalculateStylesEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(LocalFrame*);
}

namespace InspectorEventDispatchEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const Event&);
}

namespace InspectorTimeStampEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(ExecutionContext*, const String& message);
}

namespace InspectorTracingSessionIdForWorkerEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const String& sessionId, const String& workerId, WorkerThread*);
}

namespace InspectorTracingStartedInFrame {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const String& sessionId, LocalFrame*);
}

namespace InspectorSetLayerTreeId {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const String& sessionId, int layerTreeId);
}

namespace InspectorAnimationEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const Animation&);
}

namespace InspectorAnimationStateEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(const Animation&);
}

namespace InspectorHitTestEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> endData(const HitTestRequest&, const HitTestLocation&, const HitTestResult&);
}

CORE_EXPORT String toHexString(const void* p);
CORE_EXPORT void setCallStack(TracedValue*);

} // namespace blink


#endif // !defined(InspectorTraceEvents_h)
