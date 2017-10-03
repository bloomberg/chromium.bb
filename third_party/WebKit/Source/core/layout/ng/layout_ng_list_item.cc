// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/layout_ng_list_item.h"

#include "core/layout/ListMarkerText.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

LayoutNGListItem::LayoutNGListItem(Element* element)
    : LayoutNGBlockFlow(element) {
  SetInline(false);
}

bool LayoutNGListItem::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGListItem || LayoutNGBlockFlow::IsOfType(type);
}

void LayoutNGListItem::WillBeDestroyed() {
  if (marker_) {
    marker_->Destroy();
    marker_ = nullptr;
  }

  LayoutNGBlockFlow::WillBeDestroyed();
}

void LayoutNGListItem::InsertedIntoTree() {
  LayoutNGBlockFlow::InsertedIntoTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutNGListItem::WillBeRemovedFromTree() {
  LayoutNGBlockFlow::WillBeRemovedFromTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutNGListItem::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  LayoutNGBlockFlow::StyleDidChange(diff, old_style);

  UpdateMarker();
}

void LayoutNGListItem::OrdinalValueChanged() {
  if (marker_)
    UpdateMarkerText(ToLayoutText(marker_->FirstChild()));
}

void LayoutNGListItem::UpdateMarkerText(LayoutText* text) {
  DCHECK(text);
  StringBuilder marker_text_builder;
  MarkerText(&marker_text_builder, kWithSuffix);
  text->SetText(marker_text_builder.ToString().ReleaseImpl());
}

void LayoutNGListItem::UpdateMarker() {
  const ComputedStyle& style = StyleRef();
  if (style.ListStyleType() == EListStyleType::kNone) {
    if (marker_) {
      marker_->Destroy();
      marker_ = nullptr;
    }
    return;
  }

  if (!marker_)
    marker_ = LayoutBlockFlow::CreateAnonymous(&GetDocument());

  RefPtr<ComputedStyle> marker_style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(style,
                                                     EDisplay::kInlineBlock);
  marker_->SetStyle(std::move(marker_style));

  LayoutText* text = nullptr;
  if (LayoutObject* child = marker_->FirstChild()) {
    text = ToLayoutText(child);
    text->SetStyle(marker_->MutableStyle());
  } else {
    text = LayoutText::CreateEmptyAnonymous(GetDocument());
    text->SetStyle(marker_->MutableStyle());
    marker_->AddChild(text);
  }

  UpdateMarkerText(text);

  LayoutObject* first_child = FirstChild();
  if (first_child != marker_) {
    marker_->Remove();
    AddChild(marker_, FirstChild());
  }
}

int LayoutNGListItem::Value() const {
  DCHECK(GetNode());
  return ordinal_.Value(*GetNode());
}

void LayoutNGListItem::MarkerText(StringBuilder* text,
                                  MarkerTextFormat format) const {
  const ComputedStyle& style = StyleRef();
  switch (style.ListStyleType()) {
    case EListStyleType::kNone:
      break;
    case EListStyleType::kDisc:
    case EListStyleType::kCircle:
    case EListStyleType::kSquare:
      // value is ignored for these types
      text->Append(ListMarkerText::GetText(Style()->ListStyleType(), 0));
      if (format == kWithSuffix)
        text->Append(' ');
      break;
    case EListStyleType::kArabicIndic:
    case EListStyleType::kArmenian:
    case EListStyleType::kBengali:
    case EListStyleType::kCambodian:
    case EListStyleType::kCjkIdeographic:
    case EListStyleType::kCjkEarthlyBranch:
    case EListStyleType::kCjkHeavenlyStem:
    case EListStyleType::kDecimalLeadingZero:
    case EListStyleType::kDecimal:
    case EListStyleType::kDevanagari:
    case EListStyleType::kEthiopicHalehame:
    case EListStyleType::kEthiopicHalehameAm:
    case EListStyleType::kEthiopicHalehameTiEr:
    case EListStyleType::kEthiopicHalehameTiEt:
    case EListStyleType::kGeorgian:
    case EListStyleType::kGujarati:
    case EListStyleType::kGurmukhi:
    case EListStyleType::kHangul:
    case EListStyleType::kHangulConsonant:
    case EListStyleType::kHebrew:
    case EListStyleType::kHiragana:
    case EListStyleType::kHiraganaIroha:
    case EListStyleType::kKannada:
    case EListStyleType::kKatakana:
    case EListStyleType::kKatakanaIroha:
    case EListStyleType::kKhmer:
    case EListStyleType::kKoreanHangulFormal:
    case EListStyleType::kKoreanHanjaFormal:
    case EListStyleType::kKoreanHanjaInformal:
    case EListStyleType::kLao:
    case EListStyleType::kLowerAlpha:
    case EListStyleType::kLowerArmenian:
    case EListStyleType::kLowerGreek:
    case EListStyleType::kLowerLatin:
    case EListStyleType::kLowerRoman:
    case EListStyleType::kMalayalam:
    case EListStyleType::kMongolian:
    case EListStyleType::kMyanmar:
    case EListStyleType::kOriya:
    case EListStyleType::kPersian:
    case EListStyleType::kSimpChineseFormal:
    case EListStyleType::kSimpChineseInformal:
    case EListStyleType::kTelugu:
    case EListStyleType::kThai:
    case EListStyleType::kTibetan:
    case EListStyleType::kTradChineseFormal:
    case EListStyleType::kTradChineseInformal:
    case EListStyleType::kUpperAlpha:
    case EListStyleType::kUpperArmenian:
    case EListStyleType::kUpperLatin:
    case EListStyleType::kUpperRoman:
    case EListStyleType::kUrdu: {
      int value = Value();
      text->Append(ListMarkerText::GetText(Style()->ListStyleType(), value));
      if (format == kWithSuffix) {
        text->Append(ListMarkerText::Suffix(Style()->ListStyleType(), value));
        text->Append(' ');
      }
      break;
    }
  }
}

String LayoutNGListItem::MarkerTextWithoutSuffix() const {
  StringBuilder text;
  MarkerText(&text, kWithoutSuffix);
  return text.ToString();
}

}  // namespace blink
