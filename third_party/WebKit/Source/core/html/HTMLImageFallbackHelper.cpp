// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLImageFallbackHelper.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/ElementRareData.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLStyleElement.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

static bool NoImageSourceSpecified(const Element& element) {
  bool no_src_specified = !element.hasAttribute(srcAttr) ||
                          element.getAttribute(srcAttr).IsNull() ||
                          element.getAttribute(srcAttr).IsEmpty();
  bool no_srcset_specified = !element.hasAttribute(srcsetAttr) ||
                             element.getAttribute(srcsetAttr).IsNull() ||
                             element.getAttribute(srcsetAttr).IsEmpty();
  return no_src_specified && no_srcset_specified;
}

void HTMLImageFallbackHelper::CreateAltTextShadowTree(Element& element) {
  ShadowRoot& root = element.EnsureUserAgentShadowRoot();

  HTMLDivElement* container = HTMLDivElement::Create(element.GetDocument());
  root.AppendChild(container);
  container->setAttribute(idAttr, AtomicString("alttext-container"));
  container->SetInlineStyleProperty(CSSPropertyOverflow, CSSValueHidden);
  container->SetInlineStyleProperty(CSSPropertyBorderWidth, 1,
                                    CSSPrimitiveValue::UnitType::kPixels);
  container->SetInlineStyleProperty(CSSPropertyBorderStyle, CSSValueSolid);
  container->SetInlineStyleProperty(CSSPropertyBorderColor, CSSValueSilver);
  container->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueInlineBlock);
  container->SetInlineStyleProperty(CSSPropertyBoxSizing, CSSValueBorderBox);
  container->SetInlineStyleProperty(CSSPropertyPadding, 1,
                                    CSSPrimitiveValue::UnitType::kPixels);

  HTMLImageElement* broken_image =
      HTMLImageElement::Create(element.GetDocument());
  container->AppendChild(broken_image);
  broken_image->SetIsFallbackImage();
  broken_image->setAttribute(idAttr, AtomicString("alttext-image"));
  broken_image->setAttribute(widthAttr, AtomicString("16"));
  broken_image->setAttribute(heightAttr, AtomicString("16"));
  broken_image->setAttribute(alignAttr, AtomicString("left"));
  broken_image->SetInlineStyleProperty(CSSPropertyMargin, 0,
                                       CSSPrimitiveValue::UnitType::kPixels);

  HTMLDivElement* alt_text = HTMLDivElement::Create(element.GetDocument());
  container->AppendChild(alt_text);
  alt_text->setAttribute(idAttr, AtomicString("alttext"));
  alt_text->SetInlineStyleProperty(CSSPropertyOverflow, CSSValueHidden);
  alt_text->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueBlock);

  Text* text =
      Text::Create(element.GetDocument(), ToHTMLElement(element).AltText());
  alt_text->AppendChild(text);
}

PassRefPtr<ComputedStyle> HTMLImageFallbackHelper::CustomStyleForAltText(
    Element& element,
    PassRefPtr<ComputedStyle> new_style) {
  // If we have an author shadow root or have not created the UA shadow root
  // yet, bail early. We can't use ensureUserAgentShadowRoot() here because that
  // would alter the DOM tree during style recalc.
  if (element.AuthorShadowRoot() || !element.UserAgentShadowRoot())
    return new_style;

  Element* place_holder =
      element.UserAgentShadowRoot()->GetElementById("alttext-container");
  Element* broken_image =
      element.UserAgentShadowRoot()->GetElementById("alttext-image");
  // Input elements have a UA shadow root of their own. We may not have replaced
  // it with fallback content yet.
  if (!place_holder || !broken_image)
    return new_style;

  if (element.GetDocument().InQuirksMode()) {
    // Mimic the behaviour of the image host by setting symmetric dimensions if
    // only one dimension is specified.
    if (new_style->Width().IsSpecifiedOrIntrinsic() &&
        new_style->Height().IsAuto())
      new_style->SetHeight(new_style->Width());
    else if (new_style->Height().IsSpecifiedOrIntrinsic() &&
             new_style->Width().IsAuto())
      new_style->SetWidth(new_style->Height());
    if (new_style->Width().IsSpecifiedOrIntrinsic() &&
        new_style->Height().IsSpecifiedOrIntrinsic()) {
      place_holder->SetInlineStyleProperty(CSSPropertyVerticalAlign,
                                           CSSValueBaseline);
    }
  }

  // If the image has specified dimensions allow the alt-text container expand
  // to fill them.
  if (new_style->Width().IsSpecifiedOrIntrinsic() &&
      new_style->Height().IsSpecifiedOrIntrinsic()) {
    place_holder->SetInlineStyleProperty(
        CSSPropertyWidth, 100, CSSPrimitiveValue::UnitType::kPercentage);
    place_holder->SetInlineStyleProperty(
        CSSPropertyHeight, 100, CSSPrimitiveValue::UnitType::kPercentage);
  }

  // Make sure the broken image icon appears on the appropriate side of the
  // image for the element's writing direction.
  broken_image->SetInlineStyleProperty(
      CSSPropertyFloat,
      AtomicString(new_style->Direction() == TextDirection::kLtr ? "left"
                                                                 : "right"));

  // This is an <img> with no attributes, so don't display anything.
  if (NoImageSourceSpecified(element) &&
      !new_style->Width().IsSpecifiedOrIntrinsic() &&
      !new_style->Height().IsSpecifiedOrIntrinsic() &&
      ToHTMLElement(element).AltText().IsEmpty())
    new_style->SetDisplay(EDisplay::kNone);

  // This preserves legacy behaviour originally defined when alt-text was
  // managed by LayoutImage.
  if (NoImageSourceSpecified(element))
    broken_image->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
  else
    broken_image->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueInline);

  return new_style;
}

}  // namespace blink
