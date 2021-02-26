// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_scroll_timeline.h"

#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/css/css_element_offset_value.h"
#include "third_party/blink/renderer/core/css/css_id_selector_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

namespace {

bool IsIdentifier(const CSSValue* value, CSSValueID value_id) {
  if (const auto* ident = DynamicTo<CSSIdentifierValue>(value))
    return ident->GetValueID() == value_id;
  return false;
}

bool IsAuto(const CSSValue* value) {
  return IsIdentifier(value, CSSValueID::kAuto);
}

bool IsNone(const CSSValue* value) {
  return IsIdentifier(value, CSSValueID::kNone);
}

bool IsStart(const CSSValue* value) {
  return IsIdentifier(value, CSSValueID::kStart);
}

bool IsEnd(const CSSValue* value) {
  return IsIdentifier(value, CSSValueID::kEnd);
}

const cssvalue::CSSIdSelectorValue* GetIdSelectorValue(const CSSValue* value) {
  if (const auto* selector = DynamicTo<CSSFunctionValue>(value)) {
    if (selector->FunctionType() != CSSValueID::kSelector)
      return nullptr;
    DCHECK_EQ(selector->length(), 1u);
    return DynamicTo<cssvalue::CSSIdSelectorValue>(selector->Item(0));
  }
  return nullptr;
}

Element* ComputeScrollSource(Document& document, const CSSValue* value) {
  if (const auto* id = GetIdSelectorValue(value))
    return document.getElementById(id->Id());
  if (IsNone(value))
    return nullptr;
  DCHECK(!value || IsAuto(value));
  return document.scrollingElement();
}

Element* ComputeElementOffsetTarget(Document& document, const CSSValue* value) {
  if (const auto* id = GetIdSelectorValue(value))
    return document.getElementById(id->Id());
  return nullptr;
}

String ComputeElementOffsetEdge(const CSSValue* value) {
  if (!value || IsStart(value))
    return "start";
  DCHECK(IsEnd(value));
  return "end";
}

double ComputeElementOffsetThreshold(const CSSValue* value) {
  if (auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    DCHECK(primitive_value->IsNumber());
    return primitive_value->GetDoubleValue();
  }
  return 0;
}

ScrollTimelineElementBasedOffset* ComputeElementBasedOffset(
    Document& document,
    const cssvalue::CSSElementOffsetValue* value) {
  auto* offset = MakeGarbageCollected<ScrollTimelineElementBasedOffset>();
  offset->setTarget(ComputeElementOffsetTarget(document, value->Target()));
  offset->setEdge(ComputeElementOffsetEdge(value->Edge()));
  offset->setThreshold(ComputeElementOffsetThreshold(value->Threshold()));
  return offset;
}

ScrollTimeline::ScrollDirection ComputeScrollDirection(const CSSValue* value) {
  CSSValueID value_id = CSSValueID::kAuto;

  if (const auto* identifier = DynamicTo<CSSIdentifierValue>(value))
    value_id = identifier->GetValueID();

  switch (value_id) {
    case CSSValueID::kInline:
      return ScrollTimeline::Inline;
    case CSSValueID::kHorizontal:
      return ScrollTimeline::Horizontal;
    case CSSValueID::kVertical:
      return ScrollTimeline::Vertical;
    case CSSValueID::kAuto:
    case CSSValueID::kBlock:
    default:
      return ScrollTimeline::Block;
  }
}

ScrollTimelineOffset* ComputeScrollOffset(Document& document,
                                          const CSSValue* value) {
  if (auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value))
    return MakeGarbageCollected<ScrollTimelineOffset>(primitive_value);
  if (auto* offset = DynamicTo<cssvalue::CSSElementOffsetValue>(value)) {
    auto* element_based = ComputeElementBasedOffset(document, offset);
    return MakeGarbageCollected<ScrollTimelineOffset>(element_based);
  }
  DCHECK(!value || IsAuto(value));
  return MakeGarbageCollected<ScrollTimelineOffset>();
}

HeapVector<Member<ScrollTimelineOffset>>* ComputeScrollOffsets(
    Document& document,
    const CSSValue* start,
    const CSSValue* end) {
  auto* offsets =
      MakeGarbageCollected<HeapVector<Member<ScrollTimelineOffset>>>();

  bool start_is_auto = IsAuto(start) || !start;
  bool end_is_auto = IsAuto(end) || !end;

  // TODO(crbug.com/1094014): scroll_offsets will replace start and end
  // offsets once spec decision on multiple scroll offsets is finalized.
  // https://github.com/w3c/csswg-drafts/issues/4912
  if (!start_is_auto)
    offsets->push_back(ComputeScrollOffset(document, start));
  if (!end_is_auto || !start_is_auto)
    offsets->push_back(ComputeScrollOffset(document, end));

  return offsets;
}

base::Optional<double> ComputeTimeRange(const CSSValue* value) {
  if (auto* primitive = DynamicTo<CSSPrimitiveValue>(value))
    return primitive->ComputeSeconds() * 1000.0;
  // TODO(crbug.com/1097041): Support 'auto' value.
  return base::nullopt;
}

class ElementReferenceObserver : public IdTargetObserver {
 public:
  ElementReferenceObserver(Document* document,
                           const AtomicString& id,
                           CSSScrollTimeline* timeline)
      : IdTargetObserver(document->GetIdTargetObserverRegistry(), id),
        timeline_(timeline) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(timeline_);
    IdTargetObserver::Trace(visitor);
  }

 private:
  void IdTargetChanged() override {
    if (timeline_)
      timeline_->InvalidateEffectTargetStyle();
  }
  WeakMember<CSSScrollTimeline> timeline_;
};

HeapVector<Member<IdTargetObserver>> CreateElementReferenceObservers(
    Document* document,
    StyleRuleScrollTimeline* rule,
    CSSScrollTimeline* timeline) {
  HeapVector<Member<IdTargetObserver>> observers;

  if (const auto* id = GetIdSelectorValue(rule->GetSource())) {
    observers.push_back(MakeGarbageCollected<ElementReferenceObserver>(
        document, id->Id(), timeline));
  }

  // TODO(crbug.com/1094014): The 'offsets' descriptor will replace the 'start'
  // and 'end' descriptors eventually.
  HeapVector<Member<const CSSValue>> offsets = {rule->GetStart(),
                                                rule->GetEnd()};

  for (const CSSValue* offset : offsets) {
    const auto* element_offset =
        DynamicTo<cssvalue::CSSElementOffsetValue>(offset);
    if (!element_offset)
      continue;
    if (const auto* id = GetIdSelectorValue(element_offset->Target())) {
      observers.push_back(MakeGarbageCollected<ElementReferenceObserver>(
          document, id->Id(), timeline));
    }
  }

  return observers;
}

}  // anonymous namespace

CSSScrollTimeline::Options::Options(Element* element,
                                    StyleRuleScrollTimeline& rule)
    : source_(ComputeScrollSource(element->GetDocument(), rule.GetSource())),
      direction_(ComputeScrollDirection(rule.GetOrientation())),
      offsets_(ComputeScrollOffsets(element->GetDocument(),
                                    rule.GetStart(),
                                    rule.GetEnd())),
      time_range_(ComputeTimeRange(rule.GetTimeRange())),
      rule_(&rule) {}

CSSScrollTimeline::CSSScrollTimeline(Document* document, const Options& options)
    : ScrollTimeline(document,
                     options.source_,
                     options.direction_,
                     options.offsets_,
                     *options.time_range_),
      rule_(options.rule_) {
  DCHECK(options.IsValid());
  DCHECK(rule_);
  document->GetDocumentAnimations().CacheCSSScrollTimeline(*this);
}

const AtomicString& CSSScrollTimeline::Name() const {
  return rule_->GetName();
}

bool CSSScrollTimeline::Matches(const Options& options) const {
  DCHECK(options.offsets_);
  return (scrollSource() == options.source_) &&
         (GetOrientation() == options.direction_) &&
         (ScrollOffsetsEqual(*options.offsets_)) &&
         (GetTimeRange() == options.time_range_) && (rule_ == options.rule_);
}

void CSSScrollTimeline::AnimationAttached(Animation* animation) {
  ScrollTimeline::AnimationAttached(animation);
  if (AttachedAnimationsCount() == 1)
    SetObservers(CreateElementReferenceObservers(GetDocument(), rule_, this));
}

void CSSScrollTimeline::AnimationDetached(Animation* animation) {
  ScrollTimeline::AnimationDetached(animation);
  if (AttachedAnimationsCount() == 0)
    SetObservers(HeapVector<Member<IdTargetObserver>>());
}

void CSSScrollTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(rule_);
  visitor->Trace(observers_);
  ScrollTimeline::Trace(visitor);
}

void CSSScrollTimeline::SetObservers(
    HeapVector<Member<IdTargetObserver>> observers) {
  for (IdTargetObserver* observer : observers_)
    observer->Unregister();
  observers_ = std::move(observers);
}

}  // namespace blink
