/*
 * Copyright (C) 2005, 2005 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGImageLoader.h"

#include "core/dom/events/Event.h"
#include "core/svg/SVGImageElement.h"

namespace blink {

SVGImageLoader::SVGImageLoader(SVGImageElement* node) : ImageLoader(node) {}

void SVGImageLoader::DispatchLoadEvent() {
  if (GetContent()->ErrorOccurred()) {
    GetElement()->DispatchEvent(Event::Create(EventTypeNames::error));
  } else {
    SVGImageElement* image_element = ToSVGImageElement(GetElement());
    image_element->SendSVGLoadEventToSelfAndAncestorChainIfPossible();
  }
}

}  // namespace blink
