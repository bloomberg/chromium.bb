/*
 * Copyright (C) 2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>
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

#include "core/svg/SVGFEPointLightElement.h"

#include "core/svg_names.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/PointLightSource.h"

namespace blink {

inline SVGFEPointLightElement::SVGFEPointLightElement(Document& document)
    : SVGFELightElement(SVGNames::fePointLightTag, document) {}

DEFINE_NODE_FACTORY(SVGFEPointLightElement)

scoped_refptr<LightSource> SVGFEPointLightElement::GetLightSource(
    Filter* filter) const {
  return PointLightSource::Create(filter->Resolve3dPoint(GetPosition()));
}

}  // namespace blink
