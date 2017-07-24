// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/intersection_observer/IntersectionObserver.h"

#include <algorithm>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IntersectionObserverCallback.h"
#include "bindings/core/v8/V8IntersectionObserverDelegate.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/intersection_observer/ElementIntersectionObserverData.h"
#include "core/intersection_observer/IntersectionObserverController.h"
#include "core/intersection_observer/IntersectionObserverDelegate.h"
#include "core/intersection_observer/IntersectionObserverEntry.h"
#include "core/intersection_observer/IntersectionObserverInit.h"
#include "core/layout/LayoutView.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "platform/Timer.h"

namespace blink {

namespace {

// Internal implementation of IntersectionObserverDelegate when using
// IntersectionObserver with an EventCallback.
class IntersectionObserverDelegateImpl final
    : public IntersectionObserverDelegate {
  WTF_MAKE_NONCOPYABLE(IntersectionObserverDelegateImpl);

 public:
  IntersectionObserverDelegateImpl(
      ExecutionContext* context,
      std::unique_ptr<IntersectionObserver::EventCallback> callback)
      : context_(context), callback_(std::move(callback)) {}

  void Deliver(const HeapVector<Member<IntersectionObserverEntry>>& entries,
               IntersectionObserver&) override {
    (*callback_.get())(entries);
  }

  ExecutionContext* GetExecutionContext() const override { return context_; }

  DEFINE_INLINE_TRACE() {
    IntersectionObserverDelegate::Trace(visitor);
    visitor->Trace(context_);
  }

 private:
  WeakMember<ExecutionContext> context_;
  std::unique_ptr<IntersectionObserver::EventCallback> callback_;
};

void ParseRootMargin(String root_margin_parameter,
                     Vector<Length>& root_margin,
                     ExceptionState& exception_state) {
  // TODO(szager): Make sure this exact syntax and behavior is spec-ed
  // somewhere.

  // The root margin argument accepts syntax similar to that for CSS margin:
  //
  // "1px" = top/right/bottom/left
  // "1px 2px" = top/bottom left/right
  // "1px 2px 3px" = top left/right bottom
  // "1px 2px 3px 4px" = top left right bottom
  CSSTokenizer tokenizer(root_margin_parameter);
  CSSParserTokenRange token_range = tokenizer.TokenRange();
  while (token_range.Peek().GetType() != kEOFToken &&
         !exception_state.HadException()) {
    if (root_margin.size() == 4) {
      exception_state.ThrowDOMException(
          kSyntaxError, "Extra text found at the end of rootMargin.");
      break;
    }
    const CSSParserToken& token = token_range.ConsumeIncludingWhitespace();
    switch (token.GetType()) {
      case kPercentageToken:
        root_margin.push_back(Length(token.NumericValue(), kPercent));
        break;
      case kDimensionToken:
        switch (token.GetUnitType()) {
          case CSSPrimitiveValue::UnitType::kPixels:
            root_margin.push_back(
                Length(static_cast<int>(floor(token.NumericValue())), kFixed));
            break;
          case CSSPrimitiveValue::UnitType::kPercentage:
            root_margin.push_back(Length(token.NumericValue(), kPercent));
            break;
          default:
            exception_state.ThrowDOMException(
                kSyntaxError,
                "rootMargin must be specified in pixels or percent.");
        }
        break;
      default:
        exception_state.ThrowDOMException(
            kSyntaxError, "rootMargin must be specified in pixels or percent.");
    }
  }
}

void ParseThresholds(const DoubleOrDoubleSequence& threshold_parameter,
                     Vector<float>& thresholds,
                     ExceptionState& exception_state) {
  if (threshold_parameter.isDouble()) {
    thresholds.push_back(static_cast<float>(threshold_parameter.getAsDouble()));
  } else {
    for (auto threshold_value : threshold_parameter.getAsDoubleSequence())
      thresholds.push_back(static_cast<float>(threshold_value));
  }

  for (auto threshold_value : thresholds) {
    if (std::isnan(threshold_value) || threshold_value < 0.0 ||
        threshold_value > 1.0) {
      exception_state.ThrowRangeError(
          "Threshold values must be numbers between 0 and 1");
      break;
    }
  }

  std::sort(thresholds.begin(), thresholds.end());
}

}  // anonymous namespace

IntersectionObserver* IntersectionObserver::Create(
    const IntersectionObserverInit& observer_init,
    IntersectionObserverDelegate& delegate,
    ExceptionState& exception_state) {
  Element* root = observer_init.root();

  Vector<Length> root_margin;
  ParseRootMargin(observer_init.rootMargin(), root_margin, exception_state);
  if (exception_state.HadException())
    return nullptr;

  Vector<float> thresholds;
  ParseThresholds(observer_init.threshold(), thresholds, exception_state);
  if (exception_state.HadException())
    return nullptr;

  return new IntersectionObserver(delegate, root, root_margin, thresholds);
}

IntersectionObserver* IntersectionObserver::Create(
    ScriptState* script_state,
    IntersectionObserverCallback* callback,
    const IntersectionObserverInit& observer_init,
    ExceptionState& exception_state) {
  V8IntersectionObserverDelegate* delegate =
      new V8IntersectionObserverDelegate(callback, script_state);
  return Create(observer_init, *delegate, exception_state);
}

IntersectionObserver* IntersectionObserver::Create(
    const Vector<Length>& root_margin,
    const Vector<float>& thresholds,
    Document* document,
    std::unique_ptr<EventCallback> callback,
    ExceptionState& exception_state) {
  IntersectionObserverDelegateImpl* intersection_observer_delegate =
      new IntersectionObserverDelegateImpl(document, std::move(callback));
  return new IntersectionObserver(*intersection_observer_delegate, nullptr,
                                  root_margin, thresholds);
}

IntersectionObserver::IntersectionObserver(
    IntersectionObserverDelegate& delegate,
    Element* root,
    const Vector<Length>& root_margin,
    const Vector<float>& thresholds)
    : delegate_(this, &delegate),
      root_(root),
      thresholds_(thresholds),
      top_margin_(kFixed),
      right_margin_(kFixed),
      bottom_margin_(kFixed),
      left_margin_(kFixed),
      root_is_implicit_(root ? 0 : 1) {
  switch (root_margin.size()) {
    case 0:
      break;
    case 1:
      top_margin_ = right_margin_ = bottom_margin_ = left_margin_ =
          root_margin[0];
      break;
    case 2:
      top_margin_ = bottom_margin_ = root_margin[0];
      right_margin_ = left_margin_ = root_margin[1];
      break;
    case 3:
      top_margin_ = root_margin[0];
      right_margin_ = left_margin_ = root_margin[1];
      bottom_margin_ = root_margin[2];
      break;
    case 4:
      top_margin_ = root_margin[0];
      right_margin_ = root_margin[1];
      bottom_margin_ = root_margin[2];
      left_margin_ = root_margin[3];
      break;
    default:
      NOTREACHED();
      break;
  }
  if (root)
    root->EnsureIntersectionObserverData().AddObserver(*this);
  TrackingDocument().EnsureIntersectionObserverController().AddTrackedObserver(
      *this);
}

void IntersectionObserver::ClearWeakMembers(Visitor* visitor) {
  if (RootIsImplicit() || (root() && ThreadHeap::IsHeapObjectAlive(root())))
    return;
  DummyExceptionStateForTesting exception_state;
  disconnect(exception_state);
  root_ = nullptr;
}

bool IntersectionObserver::RootIsValid() const {
  return RootIsImplicit() || root();
}

Document& IntersectionObserver::TrackingDocument() const {
  if (RootIsImplicit()) {
    DCHECK(delegate_->GetExecutionContext());
    return *ToDocument(delegate_->GetExecutionContext());
  }
  DCHECK(root());
  return root()->GetDocument();
}

void IntersectionObserver::observe(Element* target,
                                   ExceptionState& exception_state) {
  if (!RootIsValid())
    return;

  if (!target || root() == target)
    return;

  LocalFrame* target_frame = target->GetDocument().GetFrame();
  if (!target_frame)
    return;

  if (target->EnsureIntersectionObserverData().GetObservationFor(*this))
    return;

  IntersectionObservation* observation =
      new IntersectionObservation(*this, *target);
  target->EnsureIntersectionObserverData().AddObservation(*observation);
  observations_.insert(observation);
  if (LocalFrameView* frame_view = target_frame->View()) {
    // The IntersectionObsever spec requires that at least one observation
    // be recorded afer observe() is called, even if the frame is throttled.
    frame_view->SetNeedsIntersectionObservation();
    frame_view->ScheduleAnimation();
  }
}

void IntersectionObserver::unobserve(Element* target,
                                     ExceptionState& exception_state) {
  if (!target || !target->IntersectionObserverData())
    return;

  if (IntersectionObservation* observation =
          target->IntersectionObserverData()->GetObservationFor(*this)) {
    observation->Disconnect();
    observations_.erase(observation);
  }
}

void IntersectionObserver::ComputeIntersectionObservations() {
  if (!RootIsValid())
    return;
  Document* delegate_document = ToDocument(delegate_->GetExecutionContext());
  if (!delegate_document)
    return;
  LocalDOMWindow* delegate_dom_window = delegate_document->domWindow();
  if (!delegate_dom_window)
    return;
  DOMHighResTimeStamp timestamp =
      DOMWindowPerformance::performance(*delegate_dom_window)->now();
  for (auto& observation : observations_)
    observation->ComputeIntersectionObservations(timestamp);
}

void IntersectionObserver::disconnect(ExceptionState& exception_state) {
  for (auto& observation : observations_)
    observation->Disconnect();
  observations_.clear();
  entries_.clear();
}

HeapVector<Member<IntersectionObserverEntry>> IntersectionObserver::takeRecords(
    ExceptionState& exception_state) {
  HeapVector<Member<IntersectionObserverEntry>> entries;
  entries.swap(entries_);
  return entries;
}

static void AppendLength(StringBuilder& string_builder, const Length& length) {
  string_builder.AppendNumber(length.IntValue());
  if (length.GetType() == kPercent)
    string_builder.Append('%');
  else
    string_builder.Append("px", 2);
}

String IntersectionObserver::rootMargin() const {
  StringBuilder string_builder;
  AppendLength(string_builder, top_margin_);
  string_builder.Append(' ');
  AppendLength(string_builder, right_margin_);
  string_builder.Append(' ');
  AppendLength(string_builder, bottom_margin_);
  string_builder.Append(' ');
  AppendLength(string_builder, left_margin_);
  return string_builder.ToString();
}

void IntersectionObserver::EnqueueIntersectionObserverEntry(
    IntersectionObserverEntry& entry) {
  entries_.push_back(&entry);
  ToDocument(delegate_->GetExecutionContext())
      ->EnsureIntersectionObserverController()
      .ScheduleIntersectionObserverForDelivery(*this);
}

unsigned IntersectionObserver::FirstThresholdGreaterThan(float ratio) const {
  unsigned result = 0;
  while (result < thresholds_.size() && thresholds_[result] <= ratio)
    ++result;
  return result;
}

void IntersectionObserver::Deliver() {
  if (entries_.IsEmpty())
    return;

  HeapVector<Member<IntersectionObserverEntry>> entries;
  entries.swap(entries_);
  delegate_->Deliver(entries, *this);
}

DEFINE_TRACE(IntersectionObserver) {
  visitor->template RegisterWeakMembers<
      IntersectionObserver, &IntersectionObserver::ClearWeakMembers>(this);
  visitor->Trace(delegate_);
  visitor->Trace(observations_);
  visitor->Trace(entries_);
}

DEFINE_TRACE_WRAPPERS(IntersectionObserver) {
  visitor->TraceWrappers(delegate_);
}

}  // namespace blink
