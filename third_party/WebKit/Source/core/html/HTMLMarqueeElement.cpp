/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2007, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/html/HTMLMarqueeElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8HTMLMarqueeElement.h"
#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/KeyframeEffectOptions.h"
#include "core/animation/StringKeyframe.h"
#include "core/animation/TimingInput.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLStyleElement.h"
#include "wtf/Noncopyable.h"
#include <cstdlib>

namespace blink {

inline HTMLMarqueeElement::HTMLMarqueeElement(Document& document)
    : HTMLElement(HTMLNames::marqueeTag, document) {
  UseCounter::count(document, UseCounter::HTMLMarqueeElement);
}

HTMLMarqueeElement* HTMLMarqueeElement::create(Document& document) {
  HTMLMarqueeElement* marqueeElement = new HTMLMarqueeElement(document);
  marqueeElement->ensureUserAgentShadowRoot();
  return marqueeElement;
}

void HTMLMarqueeElement::didAddUserAgentShadowRoot(ShadowRoot& shadowRoot) {
  Element* style = HTMLStyleElement::create(document(), false);
  style->setTextContent(
      ":host { display: inline-block; overflow: hidden;"
      "text-align: initial; white-space: nowrap; }"
      ":host([direction=\"up\"]), :host([direction=\"down\"]) { overflow: "
      "initial; overflow-y: hidden; white-space: initial; }"
      ":host > div { will-change: transform; }");
  shadowRoot.appendChild(style);

  Element* mover = HTMLDivElement::create(document());
  shadowRoot.appendChild(mover);

  mover->appendChild(HTMLContentElement::create(document()));
  m_mover = mover;
}

class HTMLMarqueeElement::RequestAnimationFrameCallback final
    : public FrameRequestCallback {
  WTF_MAKE_NONCOPYABLE(RequestAnimationFrameCallback);

 public:
  explicit RequestAnimationFrameCallback(HTMLMarqueeElement* marquee)
      : m_marquee(marquee) {}

  void handleEvent(double) override {
    m_marquee->m_continueCallbackRequestId = 0;
    m_marquee->continueAnimation();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_marquee);
    FrameRequestCallback::trace(visitor);
  }

 private:
  Member<HTMLMarqueeElement> m_marquee;
};

class HTMLMarqueeElement::AnimationFinished final : public EventListener {
  WTF_MAKE_NONCOPYABLE(AnimationFinished);

 public:
  explicit AnimationFinished(HTMLMarqueeElement* marquee)
      : EventListener(CPPEventListenerType), m_marquee(marquee) {}

  bool operator==(const EventListener& that) const override {
    return this == &that;
  }

  void handleEvent(ExecutionContext*, Event*) override {
    ++m_marquee->m_loopCount;
    m_marquee->start();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_marquee);
    EventListener::trace(visitor);
  }

 private:
  Member<HTMLMarqueeElement> m_marquee;
};

Node::InsertionNotificationRequest HTMLMarqueeElement::insertedInto(
    ContainerNode* insertionPoint) {
  HTMLElement::insertedInto(insertionPoint);

  if (isConnected())
    start();

  return InsertionDone;
}

void HTMLMarqueeElement::removedFrom(ContainerNode* insertionPoint) {
  HTMLElement::removedFrom(insertionPoint);
  if (insertionPoint->isConnected()) {
    stop();
  }
}

bool HTMLMarqueeElement::isHorizontal() const {
  Direction direction = getDirection();
  return direction != kUp && direction != kDown;
}

int HTMLMarqueeElement::scrollAmount() const {
  bool ok;
  int scrollAmount = fastGetAttribute(HTMLNames::scrollamountAttr).toInt(&ok);
  if (!ok || scrollAmount < 0)
    return kDefaultScrollAmount;
  return scrollAmount;
}

void HTMLMarqueeElement::setScrollAmount(int value,
                                         ExceptionState& exceptionState) {
  if (value < 0) {
    exceptionState.throwDOMException(
        IndexSizeError,
        "The provided value (" + String::number(value) + ") is negative.");
    return;
  }
  setIntegralAttribute(HTMLNames::scrollamountAttr, value);
}

int HTMLMarqueeElement::scrollDelay() const {
  bool ok;
  int scrollDelay = fastGetAttribute(HTMLNames::scrolldelayAttr).toInt(&ok);
  if (!ok || scrollDelay < 0)
    return kDefaultScrollDelayMS;
  return scrollDelay;
}

void HTMLMarqueeElement::setScrollDelay(int value,
                                        ExceptionState& exceptionState) {
  if (value < 0) {
    exceptionState.throwDOMException(
        IndexSizeError,
        "The provided value (" + String::number(value) + ") is negative.");
    return;
  }
  setIntegralAttribute(HTMLNames::scrolldelayAttr, value);
}

int HTMLMarqueeElement::loop() const {
  bool ok;
  int loop = fastGetAttribute(HTMLNames::loopAttr).toInt(&ok);
  if (!ok || loop <= 0)
    return kDefaultLoopLimit;
  return loop;
}

void HTMLMarqueeElement::setLoop(int value, ExceptionState& exceptionState) {
  if (value <= 0 && value != -1) {
    exceptionState.throwDOMException(
        IndexSizeError, "The provided value (" + String::number(value) +
                            ") is neither positive nor -1.");
    return;
  }
  setIntegralAttribute(HTMLNames::loopAttr, value);
}

void HTMLMarqueeElement::start() {
  if (m_continueCallbackRequestId)
    return;

  RequestAnimationFrameCallback* callback =
      new RequestAnimationFrameCallback(this);
  m_continueCallbackRequestId = document().requestAnimationFrame(callback);
}

void HTMLMarqueeElement::stop() {
  if (m_continueCallbackRequestId) {
    document().cancelAnimationFrame(m_continueCallbackRequestId);
    m_continueCallbackRequestId = 0;
    return;
  }

  if (m_player)
    m_player->pause();
}

bool HTMLMarqueeElement::isPresentationAttribute(
    const QualifiedName& attr) const {
  if (attr == HTMLNames::bgcolorAttr || attr == HTMLNames::heightAttr ||
      attr == HTMLNames::hspaceAttr || attr == HTMLNames::vspaceAttr ||
      attr == HTMLNames::widthAttr) {
    return true;
  }
  return HTMLElement::isPresentationAttribute(attr);
}

void HTMLMarqueeElement::collectStyleForPresentationAttribute(
    const QualifiedName& attr,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (attr == HTMLNames::bgcolorAttr) {
    addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
  } else if (attr == HTMLNames::heightAttr) {
    addHTMLLengthToStyle(style, CSSPropertyHeight, value);
  } else if (attr == HTMLNames::hspaceAttr) {
    addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
    addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
  } else if (attr == HTMLNames::vspaceAttr) {
    addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
    addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
  } else if (attr == HTMLNames::widthAttr) {
    addHTMLLengthToStyle(style, CSSPropertyWidth, value);
  } else {
    HTMLElement::collectStyleForPresentationAttribute(attr, value, style);
  }
}

StringKeyframeEffectModel* HTMLMarqueeElement::createEffectModel(
    const AnimationParameters& parameters) {
  StyleSheetContents* styleSheetContents =
      m_mover->document().elementSheet().contents();
  MutableStylePropertySet::SetResult setResult;

  RefPtr<StringKeyframe> keyframe1 = StringKeyframe::create();
  setResult = keyframe1->setCSSPropertyValue(
      CSSPropertyTransform, parameters.transformBegin, styleSheetContents);
  DCHECK(setResult.didParse);

  RefPtr<StringKeyframe> keyframe2 = StringKeyframe::create();
  setResult = keyframe2->setCSSPropertyValue(
      CSSPropertyTransform, parameters.transformEnd, styleSheetContents);
  DCHECK(setResult.didParse);

  return StringKeyframeEffectModel::create(
      {std::move(keyframe1), std::move(keyframe2)},
      LinearTimingFunction::shared());
}

void HTMLMarqueeElement::continueAnimation() {
  if (!shouldContinue())
    return;

  if (m_player && m_player->playState() == "paused") {
    m_player->play();
    return;
  }

  AnimationParameters parameters = getAnimationParameters();
  int scrollDelay = this->scrollDelay();
  int scrollAmount = this->scrollAmount();

  if (scrollDelay < kMinimumScrollDelayMS &&
      !fastHasAttribute(HTMLNames::truespeedAttr))
    scrollDelay = kDefaultScrollDelayMS;
  double duration = 0;
  if (scrollAmount)
    duration = parameters.distance * scrollDelay / scrollAmount;
  if (!duration)
    return;

  StringKeyframeEffectModel* effectModel = createEffectModel(parameters);
  Timing timing;
  timing.fillMode = Timing::FillMode::FORWARDS;
  TimingInput::setIterationDuration(
      timing, UnrestrictedDoubleOrString::fromUnrestrictedDouble(duration),
      ASSERT_NO_EXCEPTION);

  KeyframeEffect* keyframeEffect =
      KeyframeEffect::create(m_mover, effectModel, timing);
  Animation* player = m_mover->document().timeline().play(keyframeEffect);
  player->setId(emptyString);
  player->setOnfinish(new AnimationFinished(this));

  m_player = player;
}

bool HTMLMarqueeElement::shouldContinue() {
  int loopCount = loop();

  // By default, slide loops only once.
  if (loopCount <= 0 && getBehavior() == kSlide)
    loopCount = 1;

  if (loopCount <= 0)
    return true;
  return m_loopCount < loopCount;
}

HTMLMarqueeElement::Behavior HTMLMarqueeElement::getBehavior() const {
  const AtomicString& behavior = fastGetAttribute(HTMLNames::behaviorAttr);
  if (equalIgnoringASCIICase(behavior, "alternate"))
    return kAlternate;
  if (equalIgnoringASCIICase(behavior, "slide"))
    return kSlide;
  return kScroll;
}

HTMLMarqueeElement::Direction HTMLMarqueeElement::getDirection() const {
  const AtomicString& direction = fastGetAttribute(HTMLNames::directionAttr);
  if (equalIgnoringASCIICase(direction, "down"))
    return kDown;
  if (equalIgnoringASCIICase(direction, "up"))
    return kUp;
  if (equalIgnoringASCIICase(direction, "right"))
    return kRight;
  return kLeft;
}

HTMLMarqueeElement::Metrics HTMLMarqueeElement::getMetrics() {
  Metrics metrics;
  CSSStyleDeclaration* marqueeStyle =
      document().domWindow()->getComputedStyle(this, String());
  // For marquees that are declared inline, getComputedStyle returns "auto" for
  // width and height. Setting all the metrics to zero disables animation for
  // inline marquees.
  if (marqueeStyle->getPropertyValue("width") == "auto" &&
      marqueeStyle->getPropertyValue("height") == "auto") {
    metrics.contentHeight = 0;
    metrics.contentWidth = 0;
    metrics.marqueeWidth = 0;
    metrics.marqueeHeight = 0;
    return metrics;
  }

  if (isHorizontal()) {
    m_mover->style()->setProperty("width", "-webkit-max-content", "important",
                                  ASSERT_NO_EXCEPTION);
  } else {
    m_mover->style()->setProperty("height", "-webkit-max-content", "important",
                                  ASSERT_NO_EXCEPTION);
  }
  CSSStyleDeclaration* moverStyle =
      document().domWindow()->getComputedStyle(m_mover, String());

  metrics.contentWidth = moverStyle->getPropertyValue("width").toDouble();
  metrics.contentHeight = moverStyle->getPropertyValue("height").toDouble();
  metrics.marqueeWidth = marqueeStyle->getPropertyValue("width").toDouble();
  metrics.marqueeHeight = marqueeStyle->getPropertyValue("height").toDouble();

  if (isHorizontal()) {
    m_mover->style()->removeProperty("width", ASSERT_NO_EXCEPTION);
  } else {
    m_mover->style()->removeProperty("height", ASSERT_NO_EXCEPTION);
  }

  return metrics;
}

HTMLMarqueeElement::AnimationParameters
HTMLMarqueeElement::getAnimationParameters() {
  AnimationParameters parameters;
  Metrics metrics = getMetrics();

  double totalWidth = metrics.marqueeWidth + metrics.contentWidth;
  double totalHeight = metrics.marqueeHeight + metrics.contentHeight;

  double innerWidth = metrics.marqueeWidth - metrics.contentWidth;
  double innerHeight = metrics.marqueeHeight - metrics.contentHeight;

  switch (getBehavior()) {
    case kAlternate:
      switch (getDirection()) {
        case kRight:
          parameters.transformBegin =
              createTransform(innerWidth >= 0 ? 0 : innerWidth);
          parameters.transformEnd =
              createTransform(innerWidth >= 0 ? innerWidth : 0);
          parameters.distance = std::abs(innerWidth);
          break;
        case kUp:
          parameters.transformBegin =
              createTransform(innerHeight >= 0 ? innerHeight : 0);
          parameters.transformEnd =
              createTransform(innerHeight >= 0 ? 0 : innerHeight);
          parameters.distance = std::abs(innerHeight);
          break;
        case kDown:
          parameters.transformBegin =
              createTransform(innerHeight >= 0 ? 0 : innerHeight);
          parameters.transformEnd =
              createTransform(innerHeight >= 0 ? innerHeight : 0);
          parameters.distance = std::abs(innerHeight);
          break;
        case kLeft:
        default:
          parameters.transformBegin =
              createTransform(innerWidth >= 0 ? innerWidth : 0);
          parameters.transformEnd =
              createTransform(innerWidth >= 0 ? 0 : innerWidth);
          parameters.distance = std::abs(innerWidth);
      }

      if (m_loopCount % 2)
        std::swap(parameters.transformBegin, parameters.transformEnd);
      break;
    case kSlide:
      switch (getDirection()) {
        case kRight:
          parameters.transformBegin = createTransform(-metrics.contentWidth);
          parameters.transformEnd = createTransform(innerWidth);
          parameters.distance = metrics.marqueeWidth;
          break;
        case kUp:
          parameters.transformBegin = createTransform(metrics.marqueeHeight);
          parameters.transformEnd = "translateY(0)";
          parameters.distance = metrics.marqueeHeight;
          break;
        case kDown:
          parameters.transformBegin = createTransform(-metrics.contentHeight);
          parameters.transformEnd = createTransform(innerHeight);
          parameters.distance = metrics.marqueeHeight;
          break;
        case kLeft:
        default:
          parameters.transformBegin = createTransform(metrics.marqueeWidth);
          parameters.transformEnd = "translateX(0)";
          parameters.distance = metrics.marqueeWidth;
      }
      break;
    case kScroll:
    default:
      switch (getDirection()) {
        case kRight:
          parameters.transformBegin = createTransform(-metrics.contentWidth);
          parameters.transformEnd = createTransform(metrics.marqueeWidth);
          parameters.distance = totalWidth;
          break;
        case kUp:
          parameters.transformBegin = createTransform(metrics.marqueeHeight);
          parameters.transformEnd = createTransform(-metrics.contentHeight);
          parameters.distance = totalHeight;
          break;
        case kDown:
          parameters.transformBegin = createTransform(-metrics.contentHeight);
          parameters.transformEnd = createTransform(metrics.marqueeHeight);
          parameters.distance = totalHeight;
          break;
        case kLeft:
        default:
          parameters.transformBegin = createTransform(metrics.marqueeWidth);
          parameters.transformEnd = createTransform(-metrics.contentWidth);
          parameters.distance = totalWidth;
      }
      break;
  }

  return parameters;
}

AtomicString HTMLMarqueeElement::createTransform(double value) const {
  char axis = isHorizontal() ? 'X' : 'Y';
  return String::format("translate%c(", axis) +
         String::numberToStringECMAScript(value) + "px)";
}

DEFINE_TRACE(HTMLMarqueeElement) {
  visitor->trace(m_mover);
  visitor->trace(m_player);
  HTMLElement::trace(visitor);
}

}  // namespace blink
