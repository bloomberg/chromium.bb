// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObserver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/dom/Element.h"
#include "core/dom/ElementIntersectionObserverData.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/IntersectionObserverCallback.h"
#include "core/dom/IntersectionObserverController.h"
#include "core/dom/IntersectionObserverEntry.h"
#include "core/dom/IntersectionObserverInit.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutView.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "platform/Timer.h"
#include <algorithm>

namespace blink {

namespace {

// Internal implementation of IntersectionObserverCallback when using
// IntersectionObserver with an EventCallback.
class IntersectionObserverCallbackImpl final
    : public IntersectionObserverCallback {
  WTF_MAKE_NONCOPYABLE(IntersectionObserverCallbackImpl);

 public:
  IntersectionObserverCallbackImpl(
      ExecutionContext* context,
      std::unique_ptr<IntersectionObserver::EventCallback> callback)
      : m_context(context), m_callback(std::move(callback)) {}

  void handleEvent(const HeapVector<Member<IntersectionObserverEntry>>& entries,
                   IntersectionObserver&) override {
    (*m_callback.get())(entries);
  }

  ExecutionContext* getExecutionContext() const override { return m_context; }

  DEFINE_INLINE_TRACE() {
    IntersectionObserverCallback::trace(visitor);
    visitor->trace(m_context);
  }

 private:
  WeakMember<ExecutionContext> m_context;
  std::unique_ptr<IntersectionObserver::EventCallback> m_callback;
};

void parseRootMargin(String rootMarginParameter,
                     Vector<Length>& rootMargin,
                     ExceptionState& exceptionState) {
  // TODO(szager): Make sure this exact syntax and behavior is spec-ed
  // somewhere.

  // The root margin argument accepts syntax similar to that for CSS margin:
  //
  // "1px" = top/right/bottom/left
  // "1px 2px" = top/bottom left/right
  // "1px 2px 3px" = top left/right bottom
  // "1px 2px 3px 4px" = top left right bottom
  CSSTokenizer tokenizer(rootMarginParameter);
  CSSParserTokenRange tokenRange = tokenizer.tokenRange();
  while (tokenRange.peek().type() != EOFToken &&
         !exceptionState.hadException()) {
    if (rootMargin.size() == 4) {
      exceptionState.throwDOMException(
          SyntaxError, "Extra text found at the end of rootMargin.");
      break;
    }
    const CSSParserToken& token = tokenRange.consumeIncludingWhitespace();
    switch (token.type()) {
      case PercentageToken:
        rootMargin.push_back(Length(token.numericValue(), Percent));
        break;
      case DimensionToken:
        switch (token.unitType()) {
          case CSSPrimitiveValue::UnitType::Pixels:
            rootMargin.push_back(
                Length(static_cast<int>(floor(token.numericValue())), Fixed));
            break;
          case CSSPrimitiveValue::UnitType::Percentage:
            rootMargin.push_back(Length(token.numericValue(), Percent));
            break;
          default:
            exceptionState.throwDOMException(
                SyntaxError,
                "rootMargin must be specified in pixels or percent.");
        }
        break;
      default:
        exceptionState.throwDOMException(
            SyntaxError, "rootMargin must be specified in pixels or percent.");
    }
  }
}

void parseThresholds(const DoubleOrDoubleSequence& thresholdParameter,
                     Vector<float>& thresholds,
                     ExceptionState& exceptionState) {
  if (thresholdParameter.isDouble()) {
    thresholds.push_back(static_cast<float>(thresholdParameter.getAsDouble()));
  } else {
    for (auto thresholdValue : thresholdParameter.getAsDoubleSequence())
      thresholds.push_back(static_cast<float>(thresholdValue));
  }

  for (auto thresholdValue : thresholds) {
    if (std::isnan(thresholdValue) || thresholdValue < 0.0 ||
        thresholdValue > 1.0) {
      exceptionState.throwRangeError(
          "Threshold values must be numbers between 0 and 1");
      break;
    }
  }

  std::sort(thresholds.begin(), thresholds.end());
}

}  // anonymous namespace

IntersectionObserver* IntersectionObserver::create(
    const IntersectionObserverInit& observerInit,
    IntersectionObserverCallback& callback,
    ExceptionState& exceptionState) {
  Element* root = observerInit.root();

  Vector<Length> rootMargin;
  parseRootMargin(observerInit.rootMargin(), rootMargin, exceptionState);
  if (exceptionState.hadException())
    return nullptr;

  Vector<float> thresholds;
  parseThresholds(observerInit.threshold(), thresholds, exceptionState);
  if (exceptionState.hadException())
    return nullptr;

  return new IntersectionObserver(callback, root, rootMargin, thresholds);
}

IntersectionObserver* IntersectionObserver::create(
    const Vector<Length>& rootMargin,
    const Vector<float>& thresholds,
    Document* document,
    std::unique_ptr<EventCallback> callback,
    ExceptionState& exceptionState) {
  IntersectionObserverCallbackImpl* intersectionObserverCallback =
      new IntersectionObserverCallbackImpl(document, std::move(callback));
  return new IntersectionObserver(*intersectionObserverCallback, nullptr,
                                  rootMargin, thresholds);
}

IntersectionObserver::IntersectionObserver(
    IntersectionObserverCallback& callback,
    Element* root,
    const Vector<Length>& rootMargin,
    const Vector<float>& thresholds)
    : m_callback(&callback),
      m_root(root),
      m_thresholds(thresholds),
      m_topMargin(Fixed),
      m_rightMargin(Fixed),
      m_bottomMargin(Fixed),
      m_leftMargin(Fixed),
      m_rootIsImplicit(root ? 0 : 1) {
  switch (rootMargin.size()) {
    case 0:
      break;
    case 1:
      m_topMargin = m_rightMargin = m_bottomMargin = m_leftMargin =
          rootMargin[0];
      break;
    case 2:
      m_topMargin = m_bottomMargin = rootMargin[0];
      m_rightMargin = m_leftMargin = rootMargin[1];
      break;
    case 3:
      m_topMargin = rootMargin[0];
      m_rightMargin = m_leftMargin = rootMargin[1];
      m_bottomMargin = rootMargin[2];
      break;
    case 4:
      m_topMargin = rootMargin[0];
      m_rightMargin = rootMargin[1];
      m_bottomMargin = rootMargin[2];
      m_leftMargin = rootMargin[3];
      break;
    default:
      NOTREACHED();
      break;
  }
  if (root)
    root->ensureIntersectionObserverData().addObserver(*this);
  trackingDocument().ensureIntersectionObserverController().addTrackedObserver(
      *this);
}

void IntersectionObserver::clearWeakMembers(Visitor* visitor) {
  if (ThreadHeap::isHeapObjectAlive(root()))
    return;
  DummyExceptionStateForTesting exceptionState;
  disconnect(exceptionState);
  m_root = nullptr;
}

bool IntersectionObserver::rootIsValid() const {
  return rootIsImplicit() || root();
}

Document& IntersectionObserver::trackingDocument() const {
  if (rootIsImplicit()) {
    DCHECK(m_callback->getExecutionContext());
    return *toDocument(m_callback->getExecutionContext());
  }
  DCHECK(root());
  return root()->document();
}

void IntersectionObserver::observe(Element* target,
                                   ExceptionState& exceptionState) {
  if (!rootIsValid())
    return;

  if (!target || root() == target)
    return;

  LocalFrame* targetFrame = target->document().frame();
  if (!targetFrame)
    return;

  if (target->ensureIntersectionObserverData().getObservationFor(*this))
    return;

  bool shouldReportRootBounds = true;
  if (rootIsImplicit()) {
    Frame* rootFrame = targetFrame->tree().top();
    DCHECK(rootFrame);
    if (rootFrame != targetFrame) {
      shouldReportRootBounds =
          targetFrame->securityContext()->getSecurityOrigin()->canAccess(
              rootFrame->securityContext()->getSecurityOrigin());
    }
  }

  IntersectionObservation* observation =
      new IntersectionObservation(*this, *target, shouldReportRootBounds);
  target->ensureIntersectionObserverData().addObservation(*observation);
  m_observations.insert(observation);
  if (FrameView* frameView = targetFrame->view())
    frameView->scheduleAnimation();
}

void IntersectionObserver::unobserve(Element* target,
                                     ExceptionState& exceptionState) {
  if (!target || !target->intersectionObserverData())
    return;

  if (IntersectionObservation* observation =
          target->intersectionObserverData()->getObservationFor(*this)) {
    observation->disconnect();
    m_observations.remove(observation);
  }
}

void IntersectionObserver::computeIntersectionObservations() {
  if (!rootIsValid())
    return;
  Document* callbackDocument = toDocument(m_callback->getExecutionContext());
  if (!callbackDocument)
    return;
  LocalDOMWindow* callbackDOMWindow = callbackDocument->domWindow();
  if (!callbackDOMWindow)
    return;
  DOMHighResTimeStamp timestamp =
      DOMWindowPerformance::performance(*callbackDOMWindow)->now();
  for (auto& observation : m_observations)
    observation->computeIntersectionObservations(timestamp);
}

void IntersectionObserver::disconnect(ExceptionState& exceptionState) {
  for (auto& observation : m_observations)
    observation->disconnect();
  m_observations.clear();
  m_entries.clear();
}

HeapVector<Member<IntersectionObserverEntry>> IntersectionObserver::takeRecords(
    ExceptionState& exceptionState) {
  HeapVector<Member<IntersectionObserverEntry>> entries;
  entries.swap(m_entries);
  return entries;
}

static void appendLength(StringBuilder& stringBuilder, const Length& length) {
  stringBuilder.appendNumber(length.intValue());
  if (length.type() == Percent)
    stringBuilder.append('%');
  else
    stringBuilder.append("px", 2);
}

String IntersectionObserver::rootMargin() const {
  StringBuilder stringBuilder;
  appendLength(stringBuilder, m_topMargin);
  stringBuilder.append(' ');
  appendLength(stringBuilder, m_rightMargin);
  stringBuilder.append(' ');
  appendLength(stringBuilder, m_bottomMargin);
  stringBuilder.append(' ');
  appendLength(stringBuilder, m_leftMargin);
  return stringBuilder.toString();
}

void IntersectionObserver::enqueueIntersectionObserverEntry(
    IntersectionObserverEntry& entry) {
  m_entries.push_back(&entry);
  toDocument(m_callback->getExecutionContext())
      ->ensureIntersectionObserverController()
      .scheduleIntersectionObserverForDelivery(*this);
}

unsigned IntersectionObserver::firstThresholdGreaterThan(float ratio) const {
  unsigned result = 0;
  while (result < m_thresholds.size() && m_thresholds[result] <= ratio)
    ++result;
  return result;
}

void IntersectionObserver::deliver() {
  if (m_entries.isEmpty())
    return;

  HeapVector<Member<IntersectionObserverEntry>> entries;
  entries.swap(m_entries);
  m_callback->handleEvent(entries, *this);
}

DEFINE_TRACE(IntersectionObserver) {
  visitor->template registerWeakMembers<
      IntersectionObserver, &IntersectionObserver::clearWeakMembers>(this);
  visitor->trace(m_callback);
  visitor->trace(m_observations);
  visitor->trace(m_entries);
}

}  // namespace blink
