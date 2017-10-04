/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGDefsElement.h"

#include "core/layout/svg/LayoutSVGHiddenContainer.h"
#include "core/svg_names.h"

namespace blink {

inline SVGDefsElement::SVGDefsElement(Document& document)
    : SVGGraphicsElement(SVGNames::defsTag, document) {}

DEFINE_NODE_FACTORY(SVGDefsElement)

LayoutObject* SVGDefsElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutSVGHiddenContainer(this);
}

}  // namespace blink
