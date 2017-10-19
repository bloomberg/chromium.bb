/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#ifndef SVGAElement_h
#define SVGAElement_h

#include "core/CoreExport.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGURIReference.h"

namespace blink {

class CORE_EXPORT SVGAElement final : public SVGGraphicsElement,
                                      public SVGURIReference {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGAElement);

 public:
  DECLARE_NODE_FACTORY(SVGAElement);
  SVGAnimatedString* svgTarget() { return svg_target_.Get(); }

  virtual void Trace(blink::Visitor*);

 private:
  explicit SVGAElement(Document&);

  String title() const override;

  void SvgAttributeChanged(const QualifiedName&) override;

  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;

  void DefaultEventHandler(Event*) override;
  bool HasActivationBehavior() const override;

  bool IsLiveLink() const override { return IsLink(); }

  bool SupportsFocus() const override;
  bool ShouldHaveFocusAppearance() const final;
  void DispatchFocusEvent(
      Element* old_focused_element,
      WebFocusType,
      InputDeviceCapabilities* source_capabilities) override;
  void DispatchBlurEvent(Element* new_focused_element,
                         WebFocusType,
                         InputDeviceCapabilities* source_capabilities) override;
  bool IsMouseFocusable() const override;
  bool IsKeyboardFocusable() const override;
  bool IsURLAttribute(const Attribute&) const override;
  bool CanStartSelection() const override;
  int tabIndex() const override;

  bool WillRespondToMouseClickEvents() override;

  Member<SVGAnimatedString> svg_target_;
  bool was_focused_by_mouse_;
};

}  // namespace blink

#endif  // SVGAElement_h
