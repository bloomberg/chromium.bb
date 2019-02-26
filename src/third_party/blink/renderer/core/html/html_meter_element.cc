/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "third_party/blink/renderer/core/html/html_meter_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using namespace html_names;

HTMLMeterElement::HTMLMeterElement(Document& document)
    : LabelableElement(kMeterTag, document) {
  UseCounter::Count(document, WebFeature::kMeterElement);
}

HTMLMeterElement::~HTMLMeterElement() = default;

HTMLMeterElement* HTMLMeterElement::Create(Document& document) {
  HTMLMeterElement* meter = MakeGarbageCollected<HTMLMeterElement>(document);
  meter->EnsureUserAgentShadowRoot();
  return meter;
}

LayoutObject* HTMLMeterElement::CreateLayoutObject(const ComputedStyle& style) {
  switch (style.Appearance()) {
    case kMeterPart:
      UseCounter::Count(GetDocument(),
                        WebFeature::kMeterElementWithMeterAppearance);
      break;
    case kNoControlPart:
      UseCounter::Count(GetDocument(),
                        WebFeature::kMeterElementWithNoneAppearance);
      break;
    default:
      break;
  }
  return LabelableElement::CreateLayoutObject(style);
}

void HTMLMeterElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == kValueAttr || name == kMinAttr || name == kMaxAttr ||
      name == kLowAttr || name == kHighAttr || name == kOptimumAttr)
    DidElementStateChange();
  else
    LabelableElement::ParseAttribute(params);
}

double HTMLMeterElement::value() const {
  double value = GetFloatingPointAttribute(kValueAttr, 0);
  return std::min(std::max(value, min()), max());
}

void HTMLMeterElement::setValue(double value) {
  SetFloatingPointAttribute(kValueAttr, value);
}

double HTMLMeterElement::min() const {
  return GetFloatingPointAttribute(kMinAttr, 0);
}

void HTMLMeterElement::setMin(double min) {
  SetFloatingPointAttribute(kMinAttr, min);
}

double HTMLMeterElement::max() const {
  return std::max(GetFloatingPointAttribute(kMaxAttr, std::max(1.0, min())),
                  min());
}

void HTMLMeterElement::setMax(double max) {
  SetFloatingPointAttribute(kMaxAttr, max);
}

double HTMLMeterElement::low() const {
  double low = GetFloatingPointAttribute(kLowAttr, min());
  return std::min(std::max(low, min()), max());
}

void HTMLMeterElement::setLow(double low) {
  SetFloatingPointAttribute(kLowAttr, low);
}

double HTMLMeterElement::high() const {
  double high = GetFloatingPointAttribute(kHighAttr, max());
  return std::min(std::max(high, low()), max());
}

void HTMLMeterElement::setHigh(double high) {
  SetFloatingPointAttribute(kHighAttr, high);
}

double HTMLMeterElement::optimum() const {
  double optimum = GetFloatingPointAttribute(kOptimumAttr, (max() + min()) / 2);
  return std::min(std::max(optimum, min()), max());
}

void HTMLMeterElement::setOptimum(double optimum) {
  SetFloatingPointAttribute(kOptimumAttr, optimum);
}

HTMLMeterElement::GaugeRegion HTMLMeterElement::GetGaugeRegion() const {
  double low_value = low();
  double high_value = high();
  double the_value = value();
  double optimum_value = optimum();

  if (optimum_value < low_value) {
    // The optimum range stays under low
    if (the_value <= low_value)
      return kGaugeRegionOptimum;
    if (the_value <= high_value)
      return kGaugeRegionSuboptimal;
    return kGaugeRegionEvenLessGood;
  }

  if (high_value < optimum_value) {
    // The optimum range stays over high
    if (high_value <= the_value)
      return kGaugeRegionOptimum;
    if (low_value <= the_value)
      return kGaugeRegionSuboptimal;
    return kGaugeRegionEvenLessGood;
  }

  // The optimum range stays between high and low.
  // According to the standard, <meter> never show GaugeRegionEvenLessGood in
  // this case because the value is never less or greater than min or max.
  if (low_value <= the_value && the_value <= high_value)
    return kGaugeRegionOptimum;
  return kGaugeRegionSuboptimal;
}

double HTMLMeterElement::ValueRatio() const {
  double min = this->min();
  double max = this->max();
  double value = this->value();

  if (max <= min)
    return 0;
  return (value - min) / (max - min);
}

void HTMLMeterElement::DidElementStateChange() {
  UpdateValueAppearance(ValueRatio() * 100);
}

void HTMLMeterElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  DCHECK(!value_);

  HTMLDivElement* inner = HTMLDivElement::Create(GetDocument());
  inner->SetShadowPseudoId(AtomicString("-webkit-meter-inner-element"));
  root.AppendChild(inner);

  HTMLDivElement* bar = HTMLDivElement::Create(GetDocument());
  bar->SetShadowPseudoId(AtomicString("-webkit-meter-bar"));

  value_ = HTMLDivElement::Create(GetDocument());
  UpdateValueAppearance(0);
  bar->AppendChild(value_);

  inner->AppendChild(bar);

  HTMLDivElement* fallback = HTMLDivElement::Create(GetDocument());
  fallback->AppendChild(
      HTMLSlotElement::CreateUserAgentDefaultSlot(GetDocument()));
  fallback->SetShadowPseudoId(AtomicString("-internal-fallback"));
  root.AppendChild(fallback);
}

void HTMLMeterElement::UpdateValueAppearance(double percentage) {
  DEFINE_STATIC_LOCAL(AtomicString, optimum_pseudo_id,
                      ("-webkit-meter-optimum-value"));
  DEFINE_STATIC_LOCAL(AtomicString, suboptimum_pseudo_id,
                      ("-webkit-meter-suboptimum-value"));
  DEFINE_STATIC_LOCAL(AtomicString, even_less_good_pseudo_id,
                      ("-webkit-meter-even-less-good-value"));

  value_->SetInlineStyleProperty(CSSPropertyWidth, percentage,
                                 CSSPrimitiveValue::UnitType::kPercentage);
  switch (GetGaugeRegion()) {
    case kGaugeRegionOptimum:
      value_->SetShadowPseudoId(optimum_pseudo_id);
      break;
    case kGaugeRegionSuboptimal:
      value_->SetShadowPseudoId(suboptimum_pseudo_id);
      break;
    case kGaugeRegionEvenLessGood:
      value_->SetShadowPseudoId(even_less_good_pseudo_id);
      break;
  }
}

bool HTMLMeterElement::CanContainRangeEndPoint() const {
  GetDocument().UpdateStyleAndLayoutTreeForNode(this);
  return GetComputedStyle() && !GetComputedStyle()->HasAppearance();
}

void HTMLMeterElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(value_);
  LabelableElement::Trace(visitor);
}

}  // namespace blink
