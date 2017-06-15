/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 */

#include "core/svg/SVGAElement.h"

#include "core/SVGNames.h"
#include "core/XLinkNames.h"
#include "core/dom/Attr.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/editing/EditingUtilities.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/svg/LayoutSVGInline.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/layout/svg/LayoutSVGTransformableContainer.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/svg/animation/SVGSMILElement.h"
#include "platform/loader/fetch/ResourceRequest.h"

namespace blink {

using namespace HTMLNames;

inline SVGAElement::SVGAElement(Document& document)
    : SVGGraphicsElement(SVGNames::aTag, document),
      SVGURIReference(this),
      svg_target_(SVGAnimatedString::Create(this, SVGNames::targetAttr)),
      was_focused_by_mouse_(false) {
  AddToPropertyMap(svg_target_);
}

DEFINE_TRACE(SVGAElement) {
  visitor->Trace(svg_target_);
  SVGGraphicsElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGAElement)

String SVGAElement::title() const {
  // If the xlink:title is set (non-empty string), use it.
  const AtomicString& title = FastGetAttribute(XLinkNames::titleAttr);
  if (!title.IsEmpty())
    return title;

  // Otherwise, use the title of this element.
  return SVGElement::title();
}

void SVGAElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  // Unlike other SVG*Element classes, SVGAElement only listens to
  // SVGURIReference changes as none of the other properties changes the linking
  // behaviour for our <a> element.
  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    bool was_link = IsLink();
    SetIsLink(!HrefString().IsNull());

    if (was_link || IsLink()) {
      PseudoStateChanged(CSSSelector::kPseudoLink);
      PseudoStateChanged(CSSSelector::kPseudoVisited);
      PseudoStateChanged(CSSSelector::kPseudoAnyLink);
    }
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(attr_name);
}

LayoutObject* SVGAElement::CreateLayoutObject(const ComputedStyle&) {
  if (parentNode() && parentNode()->IsSVGElement() &&
      ToSVGElement(parentNode())->IsTextContent())
    return new LayoutSVGInline(this);

  return new LayoutSVGTransformableContainer(this);
}

void SVGAElement::DefaultEventHandler(Event* event) {
  if (IsLink()) {
    if (IsFocused() && IsEnterKeyKeydownEvent(event)) {
      event->SetDefaultHandled();
      DispatchSimulatedClick(event);
      return;
    }

    if (IsLinkClick(event)) {
      String url = StripLeadingAndTrailingHTMLSpaces(HrefString());

      if (url[0] == '#') {
        Element* target_element =
            GetTreeScope().getElementById(AtomicString(url.Substring(1)));
        if (target_element && IsSVGSMILElement(*target_element)) {
          ToSVGSMILElement(target_element)->BeginByLinkActivation();
          event->SetDefaultHandled();
          return;
        }
      }

      AtomicString target(svg_target_->CurrentValue()->Value());
      if (target.IsEmpty() && FastGetAttribute(XLinkNames::showAttr) == "new")
        target = AtomicString("_blank");
      event->SetDefaultHandled();

      LocalFrame* frame = GetDocument().GetFrame();
      if (!frame)
        return;
      FrameLoadRequest frame_request(
          &GetDocument(), ResourceRequest(GetDocument().CompleteURL(url)),
          target);
      frame_request.SetTriggeringEvent(event);
      frame->Loader().Load(frame_request);
      return;
    }
  }

  SVGGraphicsElement::DefaultEventHandler(event);
}

bool SVGAElement::HasActivationBehavior() const {
  return true;
}

int SVGAElement::tabIndex() const {
  // Skip the supportsFocus check in SVGElement.
  return Element::tabIndex();
}

bool SVGAElement::SupportsFocus() const {
  if (HasEditableStyle(*this))
    return SVGGraphicsElement::SupportsFocus();
  // If not a link we should still be able to focus the element if it has
  // tabIndex.
  return IsLink() || SVGGraphicsElement::SupportsFocus();
}

bool SVGAElement::ShouldHaveFocusAppearance() const {
  return !was_focused_by_mouse_ || SVGGraphicsElement::SupportsFocus();
}

// TODO(lanwei): Will add the InputDeviceCapabilities when SVGAElement gets
// focus later, see https://crbug.com/476530.
void SVGAElement::DispatchFocusEvent(
    Element* old_focused_element,
    WebFocusType type,
    InputDeviceCapabilities* source_capabilities) {
  if (type != kWebFocusTypePage)
    was_focused_by_mouse_ = type == kWebFocusTypeMouse;
  SVGGraphicsElement::DispatchFocusEvent(old_focused_element, type,
                                         source_capabilities);
}

void SVGAElement::DispatchBlurEvent(
    Element* new_focused_element,
    WebFocusType type,
    InputDeviceCapabilities* source_capabilities) {
  if (type != kWebFocusTypePage)
    was_focused_by_mouse_ = false;
  SVGGraphicsElement::DispatchBlurEvent(new_focused_element, type,
                                        source_capabilities);
}

bool SVGAElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName().LocalName() == hrefAttr ||
         SVGGraphicsElement::IsURLAttribute(attribute);
}

bool SVGAElement::IsMouseFocusable() const {
  if (IsLink())
    return SupportsFocus();

  return SVGElement::IsMouseFocusable();
}

bool SVGAElement::IsKeyboardFocusable() const {
  if (IsFocusable() && Element::SupportsFocus())
    return SVGElement::IsKeyboardFocusable();
  if (IsLink() && !GetDocument().GetPage()->GetChromeClient().TabsToLinks())
    return false;
  return SVGElement::IsKeyboardFocusable();
}

bool SVGAElement::CanStartSelection() const {
  if (!IsLink())
    return SVGElement::CanStartSelection();
  return HasEditableStyle(*this);
}

bool SVGAElement::WillRespondToMouseClickEvents() {
  return IsLink() || SVGGraphicsElement::WillRespondToMouseClickEvents();
}

}  // namespace blink
