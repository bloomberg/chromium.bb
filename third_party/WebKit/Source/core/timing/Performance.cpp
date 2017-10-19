/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/timing/Performance.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/origin_trials/origin_trials.h"
#include "core/timing/PerformanceTiming.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/runtime_enabled_features.h"

static const double kLongTaskObserverThreshold = 0.05;

static const char kUnknownAttribution[] = "unknown";
static const char kAmbiguousAttribution[] = "multiple-contexts";
static const char kSameOriginAttribution[] = "same-origin";
static const char kSameOriginSelfAttribution[] = "self";
static const char kSameOriginAncestorAttribution[] = "same-origin-ancestor";
static const char kSameOriginDescendantAttribution[] = "same-origin-descendant";
static const char kCrossOriginAncestorAttribution[] = "cross-origin-ancestor";
static const char kCrossOriginDescendantAttribution[] =
    "cross-origin-descendant";
static const char kCrossOriginAttribution[] = "cross-origin-unreachable";

namespace blink {

namespace {

String GetFrameAttribute(HTMLFrameOwnerElement* frame_owner,
                         const QualifiedName& attr_name,
                         bool truncate) {
  String attr_value;
  if (frame_owner->hasAttribute(attr_name)) {
    attr_value = frame_owner->getAttribute(attr_name);
    if (truncate && attr_value.length() > 100)
      attr_value = attr_value.Substring(0, 100);  // Truncate to 100 chars
  }
  return attr_value;
}

const char* SameOriginAttribution(Frame* observer_frame, Frame* culprit_frame) {
  if (observer_frame == culprit_frame)
    return kSameOriginSelfAttribution;
  if (observer_frame->Tree().IsDescendantOf(culprit_frame))
    return kSameOriginAncestorAttribution;
  if (culprit_frame->Tree().IsDescendantOf(observer_frame))
    return kSameOriginDescendantAttribution;
  return kSameOriginAttribution;
}

bool IsSameOrigin(String key) {
  return key == kSameOriginAttribution ||
         key == kSameOriginDescendantAttribution ||
         key == kSameOriginAncestorAttribution ||
         key == kSameOriginSelfAttribution;
}

}  // namespace

static double ToTimeOrigin(LocalDOMWindow* window) {
  Document* document = window->document();
  if (!document)
    return 0.0;

  DocumentLoader* loader = document->Loader();
  if (!loader)
    return 0.0;

  return loader->GetTiming().ReferenceMonotonicTime();
}

Performance::Performance(LocalDOMWindow* window)
    : PerformanceBase(ToTimeOrigin(window),
                      TaskRunnerHelper::Get(TaskType::kPerformanceTimeline,
                                            window->document())),
      DOMWindowClient(window) {}

Performance::~Performance() {
}

ExecutionContext* Performance::GetExecutionContext() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetDocument();
}

MemoryInfo* Performance::memory() {
  return MemoryInfo::Create();
}

PerformanceNavigation* Performance::navigation() const {
  if (!navigation_)
    navigation_ = PerformanceNavigation::Create(GetFrame());

  return navigation_.Get();
}

PerformanceTiming* Performance::timing() const {
  if (!timing_)
    timing_ = PerformanceTiming::Create(GetFrame());

  return timing_.Get();
}

PerformanceNavigationTiming* Performance::CreateNavigationTimingInstance() {
  if (!RuntimeEnabledFeatures::PerformanceNavigationTiming2Enabled())
    return nullptr;
  if (!GetFrame())
    return nullptr;
  const DocumentLoader* document_loader =
      GetFrame()->Loader().GetDocumentLoader();
  DCHECK(document_loader);
  ResourceTimingInfo* info = document_loader->GetNavigationTimingInfo();
  if (!info)
    return nullptr;
  PerformanceServerTimingVector serverTiming =
      PerformanceServerTiming::ParseServerTiming(
          *info, PerformanceServerTiming::ShouldAllowTimingDetails::Yes);
  if (serverTiming.size()) {
    UseCounter::Count(GetFrame(), WebFeature::kPerformanceServerTiming);
  }
  return new PerformanceNavigationTiming(GetFrame(), info, GetTimeOrigin(),
                                         serverTiming);
}

void Performance::UpdateLongTaskInstrumentation() {
  DCHECK(GetFrame());
  if (!GetFrame()->GetDocument())
    return;

  if (HasObserverFor(PerformanceEntry::kLongTask)) {
    UseCounter::Count(&GetFrame()->LocalFrameRoot(),
                      WebFeature::kLongTaskObserver);
    GetFrame()->GetPerformanceMonitor()->Subscribe(
        PerformanceMonitor::kLongTask, kLongTaskObserverThreshold, this);
  } else {
    GetFrame()->GetPerformanceMonitor()->UnsubscribeAll(this);
  }
}

ScriptValue Performance::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.Add("timing", timing()->toJSONForBinding(script_state));
  result.Add("navigation", navigation()->toJSONForBinding(script_state));
  return result.GetScriptValue();
}

void Performance::Trace(blink::Visitor* visitor) {
  visitor->Trace(navigation_);
  visitor->Trace(timing_);
  PerformanceBase::Trace(visitor);
  PerformanceMonitor::Client::Trace(visitor);
  DOMWindowClient::Trace(visitor);
}

static bool CanAccessOrigin(Frame* frame1, Frame* frame2) {
  SecurityOrigin* security_origin1 =
      frame1->GetSecurityContext()->GetSecurityOrigin();
  SecurityOrigin* security_origin2 =
      frame2->GetSecurityContext()->GetSecurityOrigin();
  return security_origin1->CanAccess(security_origin2);
}

/**
 * Report sanitized name based on cross-origin policy.
 * See detailed Security doc here: http://bit.ly/2duD3F7
 */
// static
std::pair<String, DOMWindow*> Performance::SanitizedAttribution(
    ExecutionContext* task_context,
    bool has_multiple_contexts,
    LocalFrame* observer_frame) {
  if (has_multiple_contexts) {
    // Unable to attribute, multiple script execution contents were involved.
    return std::make_pair(kAmbiguousAttribution, nullptr);
  }

  if (!task_context || !task_context->IsDocument() ||
      !ToDocument(task_context)->GetFrame()) {
    // Unable to attribute as no script was involved.
    return std::make_pair(kUnknownAttribution, nullptr);
  }

  // Exactly one culprit location, attribute based on origin boundary.
  Frame* culprit_frame = ToDocument(task_context)->GetFrame();
  DCHECK(culprit_frame);
  if (CanAccessOrigin(observer_frame, culprit_frame)) {
    // From accessible frames or same origin, return culprit location URL.
    return std::make_pair(SameOriginAttribution(observer_frame, culprit_frame),
                          culprit_frame->DomWindow());
  }
  // For cross-origin, if the culprit is the descendant or ancestor of
  // observer then indicate the *closest* cross-origin frame between
  // the observer and the culprit, in the corresponding direction.
  if (culprit_frame->Tree().IsDescendantOf(observer_frame)) {
    // If the culprit is a descendant of the observer, then walk up the tree
    // from culprit to observer, and report the *last* cross-origin (from
    // observer) frame.  If no intermediate cross-origin frame is found, then
    // report the culprit directly.
    Frame* last_cross_origin_frame = culprit_frame;
    for (Frame* frame = culprit_frame; frame != observer_frame;
         frame = frame->Tree().Parent()) {
      if (!CanAccessOrigin(observer_frame, frame)) {
        last_cross_origin_frame = frame;
      }
    }
    return std::make_pair(kCrossOriginDescendantAttribution,
                          last_cross_origin_frame->DomWindow());
  }
  if (observer_frame->Tree().IsDescendantOf(culprit_frame)) {
    return std::make_pair(kCrossOriginAncestorAttribution, nullptr);
  }
  return std::make_pair(kCrossOriginAttribution, nullptr);
}

void Performance::ReportLongTask(
    double start_time,
    double end_time,
    ExecutionContext* task_context,
    bool has_multiple_contexts,
    const SubTaskAttribution::EntriesVector& sub_task_attributions) {
  if (!GetFrame())
    return;
  std::pair<String, DOMWindow*> attribution = Performance::SanitizedAttribution(
      task_context, has_multiple_contexts, GetFrame());
  DOMWindow* culprit_dom_window = attribution.second;
  SubTaskAttribution::EntriesVector empty_vector;
  if (!culprit_dom_window || !culprit_dom_window->GetFrame() ||
      !culprit_dom_window->GetFrame()->DeprecatedLocalOwner()) {
    AddLongTaskTiming(
        start_time, end_time, attribution.first, g_empty_string, g_empty_string,
        g_empty_string,
        IsSameOrigin(attribution.first) ? sub_task_attributions : empty_vector);
  } else {
    HTMLFrameOwnerElement* frame_owner =
        culprit_dom_window->GetFrame()->DeprecatedLocalOwner();
    AddLongTaskTiming(
        start_time, end_time, attribution.first,
        GetFrameAttribute(frame_owner, HTMLNames::srcAttr, false),
        GetFrameAttribute(frame_owner, HTMLNames::idAttr, false),
        GetFrameAttribute(frame_owner, HTMLNames::nameAttr, true),
        IsSameOrigin(attribution.first) ? sub_task_attributions : empty_vector);
  }
}

}  // namespace blink
