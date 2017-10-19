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

#include <cstdlib>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8HTMLMarqueeElement.h"
#include "core/CSSPropertyNames.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/KeyframeEffectOptions.h"
#include "core/animation/StringKeyframe.h"
#include "core/animation/TimingInput.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/FrameRequestCallbackCollection.h"
#include "core/dom/ShadowRoot.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html_names.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

inline HTMLMarqueeElement::HTMLMarqueeElement(Document& document)
    : HTMLElement(HTMLNames::marqueeTag, document) {
  UseCounter::Count(document, WebFeature::kHTMLMarqueeElement);
}

HTMLMarqueeElement* HTMLMarqueeElement::Create(Document& document) {
  HTMLMarqueeElement* marquee_element = new HTMLMarqueeElement(document);
  marquee_element->EnsureUserAgentShadowRoot();
  return marquee_element;
}

void HTMLMarqueeElement::DidAddUserAgentShadowRoot(ShadowRoot& shadow_root) {
  Element* style = HTMLStyleElement::Create(GetDocument(), false);
  style->setTextContent(
      ":host { display: inline-block; overflow: hidden;"
      "text-align: initial; white-space: nowrap; }"
      ":host([direction=\"up\"]), :host([direction=\"down\"]) { overflow: "
      "initial; overflow-y: hidden; white-space: initial; }"
      ":host > div { will-change: transform; }");
  shadow_root.AppendChild(style);

  Element* mover = HTMLDivElement::Create(GetDocument());
  shadow_root.AppendChild(mover);

  mover->AppendChild(HTMLContentElement::Create(GetDocument()));
  mover_ = mover;
}

class HTMLMarqueeElement::RequestAnimationFrameCallback final
    : public FrameRequestCallbackCollection::FrameCallback {
  WTF_MAKE_NONCOPYABLE(RequestAnimationFrameCallback);

 public:
  explicit RequestAnimationFrameCallback(HTMLMarqueeElement* marquee)
      : marquee_(marquee) {}

  void Invoke(double) override {
    marquee_->continue_callback_request_id_ = 0;
    marquee_->ContinueAnimation();
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(marquee_);
    FrameRequestCallbackCollection::FrameCallback::Trace(visitor);
  }

 private:
  Member<HTMLMarqueeElement> marquee_;
};

class HTMLMarqueeElement::AnimationFinished final : public EventListener {
  WTF_MAKE_NONCOPYABLE(AnimationFinished);

 public:
  explicit AnimationFinished(HTMLMarqueeElement* marquee)
      : EventListener(kCPPEventListenerType), marquee_(marquee) {}

  bool operator==(const EventListener& that) const override {
    return this == &that;
  }

  void handleEvent(ExecutionContext*, Event*) override {
    ++marquee_->loop_count_;
    marquee_->start();
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(marquee_);
    EventListener::Trace(visitor);
  }

 private:
  Member<HTMLMarqueeElement> marquee_;
};

Node::InsertionNotificationRequest HTMLMarqueeElement::InsertedInto(
    ContainerNode* insertion_point) {
  HTMLElement::InsertedInto(insertion_point);

  if (isConnected())
    start();

  return kInsertionDone;
}

void HTMLMarqueeElement::RemovedFrom(ContainerNode* insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  if (insertion_point->isConnected()) {
    stop();
  }
}

bool HTMLMarqueeElement::IsHorizontal() const {
  Direction direction = GetDirection();
  return direction != kUp && direction != kDown;
}

unsigned HTMLMarqueeElement::scrollAmount() const {
  unsigned scroll_amount = 0;
  AtomicString value = FastGetAttribute(HTMLNames::scrollamountAttr);
  if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, scroll_amount) ||
      scroll_amount > 0x7fffffffu)
    return kDefaultScrollAmount;
  return scroll_amount;
}

void HTMLMarqueeElement::setScrollAmount(unsigned value) {
  SetUnsignedIntegralAttribute(HTMLNames::scrollamountAttr, value,
                               kDefaultScrollAmount);
}

unsigned HTMLMarqueeElement::scrollDelay() const {
  unsigned scroll_delay = 0;
  AtomicString value = FastGetAttribute(HTMLNames::scrolldelayAttr);
  if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, scroll_delay) ||
      scroll_delay > 0x7fffffffu)
    return kDefaultScrollDelayMS;
  return scroll_delay;
}

void HTMLMarqueeElement::setScrollDelay(unsigned value) {
  SetUnsignedIntegralAttribute(HTMLNames::scrolldelayAttr, value,
                               kDefaultScrollDelayMS);
}

int HTMLMarqueeElement::loop() const {
  bool ok;
  int loop = FastGetAttribute(HTMLNames::loopAttr).ToInt(&ok);
  if (!ok || loop <= 0)
    return kDefaultLoopLimit;
  return loop;
}

void HTMLMarqueeElement::setLoop(int value, ExceptionState& exception_state) {
  if (value <= 0 && value != -1) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The provided value (" + String::Number(value) +
                             ") is neither positive nor -1.");
    return;
  }
  SetIntegralAttribute(HTMLNames::loopAttr, value);
}

void HTMLMarqueeElement::start() {
  if (continue_callback_request_id_)
    return;

  RequestAnimationFrameCallback* callback =
      new RequestAnimationFrameCallback(this);
  continue_callback_request_id_ = GetDocument().RequestAnimationFrame(callback);
}

void HTMLMarqueeElement::stop() {
  if (continue_callback_request_id_) {
    GetDocument().CancelAnimationFrame(continue_callback_request_id_);
    continue_callback_request_id_ = 0;
    return;
  }

  if (player_)
    player_->pause();
}

bool HTMLMarqueeElement::IsPresentationAttribute(
    const QualifiedName& attr) const {
  if (attr == HTMLNames::bgcolorAttr || attr == HTMLNames::heightAttr ||
      attr == HTMLNames::hspaceAttr || attr == HTMLNames::vspaceAttr ||
      attr == HTMLNames::widthAttr) {
    return true;
  }
  return HTMLElement::IsPresentationAttribute(attr);
}

void HTMLMarqueeElement::CollectStyleForPresentationAttribute(
    const QualifiedName& attr,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (attr == HTMLNames::bgcolorAttr) {
    AddHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
  } else if (attr == HTMLNames::heightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyHeight, value);
  } else if (attr == HTMLNames::hspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
  } else if (attr == HTMLNames::vspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
  } else if (attr == HTMLNames::widthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyWidth, value);
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(attr, value, style);
  }
}

StringKeyframeEffectModel* HTMLMarqueeElement::CreateEffectModel(
    const AnimationParameters& parameters) {
  StyleSheetContents* style_sheet_contents =
      mover_->GetDocument().ElementSheet().Contents();
  MutableStylePropertySet::SetResult set_result;

  scoped_refptr<StringKeyframe> keyframe1 = StringKeyframe::Create();
  set_result = keyframe1->SetCSSPropertyValue(
      CSSPropertyTransform, parameters.transform_begin, style_sheet_contents);
  DCHECK(set_result.did_parse);

  scoped_refptr<StringKeyframe> keyframe2 = StringKeyframe::Create();
  set_result = keyframe2->SetCSSPropertyValue(
      CSSPropertyTransform, parameters.transform_end, style_sheet_contents);
  DCHECK(set_result.did_parse);

  return StringKeyframeEffectModel::Create(
      {std::move(keyframe1), std::move(keyframe2)},
      LinearTimingFunction::Shared());
}

void HTMLMarqueeElement::ContinueAnimation() {
  if (!ShouldContinue())
    return;

  if (player_ && player_->playState() == "paused") {
    player_->play();
    return;
  }

  AnimationParameters parameters = GetAnimationParameters();
  int scroll_delay = this->scrollDelay();
  int scroll_amount = this->scrollAmount();

  if (scroll_delay < kMinimumScrollDelayMS &&
      !FastHasAttribute(HTMLNames::truespeedAttr))
    scroll_delay = kDefaultScrollDelayMS;
  double duration = 0;
  if (scroll_amount)
    duration = parameters.distance * scroll_delay / scroll_amount;
  if (!duration)
    return;

  StringKeyframeEffectModel* effect_model = CreateEffectModel(parameters);
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  TimingInput::SetIterationDuration(
      timing, UnrestrictedDoubleOrString::FromUnrestrictedDouble(duration),
      ASSERT_NO_EXCEPTION);

  KeyframeEffect* keyframe_effect =
      KeyframeEffect::Create(mover_, effect_model, timing);
  Animation* player = mover_->GetDocument().Timeline().Play(keyframe_effect);
  player->setId(g_empty_string);
  player->setOnfinish(new AnimationFinished(this));

  player_ = player;
}

bool HTMLMarqueeElement::ShouldContinue() {
  int loop_count = loop();

  // By default, slide loops only once.
  if (loop_count <= 0 && GetBehavior() == kSlide)
    loop_count = 1;

  if (loop_count <= 0)
    return true;
  return loop_count_ < loop_count;
}

HTMLMarqueeElement::Behavior HTMLMarqueeElement::GetBehavior() const {
  const AtomicString& behavior = FastGetAttribute(HTMLNames::behaviorAttr);
  if (EqualIgnoringASCIICase(behavior, "alternate"))
    return kAlternate;
  if (EqualIgnoringASCIICase(behavior, "slide"))
    return kSlide;
  return kScroll;
}

HTMLMarqueeElement::Direction HTMLMarqueeElement::GetDirection() const {
  const AtomicString& direction = FastGetAttribute(HTMLNames::directionAttr);
  if (EqualIgnoringASCIICase(direction, "down"))
    return kDown;
  if (EqualIgnoringASCIICase(direction, "up"))
    return kUp;
  if (EqualIgnoringASCIICase(direction, "right"))
    return kRight;
  return kLeft;
}

HTMLMarqueeElement::Metrics HTMLMarqueeElement::GetMetrics() {
  Metrics metrics;
  CSSStyleDeclaration* marquee_style =
      GetDocument().domWindow()->getComputedStyle(this);
  // For marquees that are declared inline, getComputedStyle returns "auto" for
  // width and height. Setting all the metrics to zero disables animation for
  // inline marquees.
  if (marquee_style->getPropertyValue("width") == "auto" &&
      marquee_style->getPropertyValue("height") == "auto") {
    metrics.content_height = 0;
    metrics.content_width = 0;
    metrics.marquee_width = 0;
    metrics.marquee_height = 0;
    return metrics;
  }

  if (IsHorizontal()) {
    mover_->style()->setProperty("width", "-webkit-max-content", "important",
                                 ASSERT_NO_EXCEPTION);
  } else {
    mover_->style()->setProperty("height", "-webkit-max-content", "important",
                                 ASSERT_NO_EXCEPTION);
  }
  CSSStyleDeclaration* mover_style =
      GetDocument().domWindow()->getComputedStyle(mover_);

  metrics.content_width = mover_style->getPropertyValue("width").ToDouble();
  metrics.content_height = mover_style->getPropertyValue("height").ToDouble();
  metrics.marquee_width = marquee_style->getPropertyValue("width").ToDouble();
  metrics.marquee_height = marquee_style->getPropertyValue("height").ToDouble();

  if (IsHorizontal()) {
    mover_->style()->removeProperty("width", ASSERT_NO_EXCEPTION);
  } else {
    mover_->style()->removeProperty("height", ASSERT_NO_EXCEPTION);
  }

  return metrics;
}

HTMLMarqueeElement::AnimationParameters
HTMLMarqueeElement::GetAnimationParameters() {
  AnimationParameters parameters;
  Metrics metrics = GetMetrics();

  double total_width = metrics.marquee_width + metrics.content_width;
  double total_height = metrics.marquee_height + metrics.content_height;

  double inner_width = metrics.marquee_width - metrics.content_width;
  double inner_height = metrics.marquee_height - metrics.content_height;

  switch (GetBehavior()) {
    case kAlternate:
      switch (GetDirection()) {
        case kRight:
          parameters.transform_begin =
              CreateTransform(inner_width >= 0 ? 0 : inner_width);
          parameters.transform_end =
              CreateTransform(inner_width >= 0 ? inner_width : 0);
          parameters.distance = std::abs(inner_width);
          break;
        case kUp:
          parameters.transform_begin =
              CreateTransform(inner_height >= 0 ? inner_height : 0);
          parameters.transform_end =
              CreateTransform(inner_height >= 0 ? 0 : inner_height);
          parameters.distance = std::abs(inner_height);
          break;
        case kDown:
          parameters.transform_begin =
              CreateTransform(inner_height >= 0 ? 0 : inner_height);
          parameters.transform_end =
              CreateTransform(inner_height >= 0 ? inner_height : 0);
          parameters.distance = std::abs(inner_height);
          break;
        case kLeft:
        default:
          parameters.transform_begin =
              CreateTransform(inner_width >= 0 ? inner_width : 0);
          parameters.transform_end =
              CreateTransform(inner_width >= 0 ? 0 : inner_width);
          parameters.distance = std::abs(inner_width);
      }

      if (loop_count_ % 2)
        std::swap(parameters.transform_begin, parameters.transform_end);
      break;
    case kSlide:
      switch (GetDirection()) {
        case kRight:
          parameters.transform_begin = CreateTransform(-metrics.content_width);
          parameters.transform_end = CreateTransform(inner_width);
          parameters.distance = metrics.marquee_width;
          break;
        case kUp:
          parameters.transform_begin = CreateTransform(metrics.marquee_height);
          parameters.transform_end = "translateY(0)";
          parameters.distance = metrics.marquee_height;
          break;
        case kDown:
          parameters.transform_begin = CreateTransform(-metrics.content_height);
          parameters.transform_end = CreateTransform(inner_height);
          parameters.distance = metrics.marquee_height;
          break;
        case kLeft:
        default:
          parameters.transform_begin = CreateTransform(metrics.marquee_width);
          parameters.transform_end = "translateX(0)";
          parameters.distance = metrics.marquee_width;
      }
      break;
    case kScroll:
    default:
      switch (GetDirection()) {
        case kRight:
          parameters.transform_begin = CreateTransform(-metrics.content_width);
          parameters.transform_end = CreateTransform(metrics.marquee_width);
          parameters.distance = total_width;
          break;
        case kUp:
          parameters.transform_begin = CreateTransform(metrics.marquee_height);
          parameters.transform_end = CreateTransform(-metrics.content_height);
          parameters.distance = total_height;
          break;
        case kDown:
          parameters.transform_begin = CreateTransform(-metrics.content_height);
          parameters.transform_end = CreateTransform(metrics.marquee_height);
          parameters.distance = total_height;
          break;
        case kLeft:
        default:
          parameters.transform_begin = CreateTransform(metrics.marquee_width);
          parameters.transform_end = CreateTransform(-metrics.content_width);
          parameters.distance = total_width;
      }
      break;
  }

  return parameters;
}

AtomicString HTMLMarqueeElement::CreateTransform(double value) const {
  char axis = IsHorizontal() ? 'X' : 'Y';
  return String::Format("translate%c(", axis) +
         String::NumberToStringECMAScript(value) + "px)";
}

void HTMLMarqueeElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(mover_);
  visitor->Trace(player_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
