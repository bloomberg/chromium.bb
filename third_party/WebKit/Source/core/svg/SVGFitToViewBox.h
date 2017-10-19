/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#ifndef SVGFitToViewBox_h
#define SVGFitToViewBox_h

#include "core/svg/SVGAnimatedPreserveAspectRatio.h"
#include "core/svg/SVGAnimatedRect.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "core/svg/SVGRect.h"
#include "platform/heap/Handle.h"

namespace blink {

class AffineTransform;
class QualifiedName;

class SVGFitToViewBox : public GarbageCollectedMixin {
 public:
  static AffineTransform ViewBoxToViewTransform(const FloatRect& view_box_rect,
                                                SVGPreserveAspectRatio*,
                                                float view_width,
                                                float view_height);

  static bool IsKnownAttribute(const QualifiedName&);

  bool HasValidViewBox() const { return view_box_->CurrentValue()->IsValid(); }
  bool HasEmptyViewBox() const {
    return view_box_->CurrentValue()->IsValid() &&
           view_box_->CurrentValue()->Value().IsEmpty();
  }

  // JS API
  SVGAnimatedRect* viewBox() const { return view_box_.Get(); }
  SVGAnimatedPreserveAspectRatio* preserveAspectRatio() const {
    return preserve_aspect_ratio_.Get();
  }

  virtual void Trace(blink::Visitor*);

 protected:
  explicit SVGFitToViewBox(SVGElement*);

 private:
  Member<SVGAnimatedRect> view_box_;
  Member<SVGAnimatedPreserveAspectRatio> preserve_aspect_ratio_;
};

}  // namespace blink

#endif  // SVGFitToViewBox_h
