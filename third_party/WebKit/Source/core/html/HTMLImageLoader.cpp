/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#include "core/html/HTMLImageLoader.h"

#include "core/HTMLNames.h"
#include "core/dom/Element.h"
#include "core/dom/events/Event.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"

namespace blink {

using namespace HTMLNames;

HTMLImageLoader::HTMLImageLoader(Element* element) : ImageLoader(element) {}

HTMLImageLoader::~HTMLImageLoader() {}

void HTMLImageLoader::DispatchLoadEvent() {
  RESOURCE_LOADING_DVLOG(1) << "HTMLImageLoader::dispatchLoadEvent " << this;

  // HTMLVideoElement uses this class to load the poster image, but it should
  // not fire events for loading or failure.
  if (isHTMLVideoElement(*GetElement()))
    return;

  bool error_occurred = GetImage()->ErrorOccurred();
  if (isHTMLObjectElement(*GetElement()) && !error_occurred) {
    // An <object> considers a 404 to be an error and should fire onerror.
    error_occurred = (GetImage()->GetResponse().HttpStatusCode() >= 400);
  }
  GetElement()->DispatchEvent(Event::Create(
      error_occurred ? EventTypeNames::error : EventTypeNames::load));
}

void HTMLImageLoader::NoImageResourceToLoad() {
  // FIXME: Use fallback content even when there is no alt-text. The only
  // blocker is the large amount of rebaselining it requires.
  if (ToHTMLElement(GetElement())->AltText().IsEmpty())
    return;

  if (isHTMLImageElement(GetElement()))
    toHTMLImageElement(GetElement())->EnsureCollapsedOrFallbackContent();
  else if (isHTMLInputElement(GetElement()))
    toHTMLInputElement(GetElement())->EnsureFallbackContent();
}

void HTMLImageLoader::ImageNotifyFinished(ImageResourceContent*) {
  ImageResourceContent* cached_image = GetImage();
  Element* element = this->GetElement();
  ImageLoader::ImageNotifyFinished(cached_image);

  bool load_error = cached_image->ErrorOccurred();
  if (isHTMLImageElement(*element)) {
    if (load_error)
      toHTMLImageElement(element)->EnsureCollapsedOrFallbackContent();
    else
      toHTMLImageElement(element)->EnsurePrimaryContent();
  }

  if (isHTMLInputElement(*element)) {
    if (load_error)
      toHTMLInputElement(element)->EnsureFallbackContent();
    else
      toHTMLInputElement(element)->EnsurePrimaryContent();
  }

  if ((load_error || cached_image->GetResponse().HttpStatusCode() >= 400) &&
      isHTMLObjectElement(*element))
    toHTMLObjectElement(element)->RenderFallbackContent();
}

}  // namespace blink
