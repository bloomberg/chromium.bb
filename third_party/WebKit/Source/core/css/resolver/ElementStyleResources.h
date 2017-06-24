/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 *
 */

#ifndef ElementStyleResources_h
#define ElementStyleResources_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSPropertyIDTemplates.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class CSSImageGeneratorValue;
class CSSImageSetValue;
class CSSImageValue;
class CSSURIValue;
class CSSValue;
class ComputedStyle;
class Document;
class SVGElementProxy;
class StyleImage;
class StylePendingImage;

// Holds information about resources, requested by stylesheets.
// Lifetime: per-element style resolve.
class ElementStyleResources {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ElementStyleResources);

 public:
  ElementStyleResources(Document&, float device_scale_factor);

  StyleImage* GetStyleImage(CSSPropertyID, const CSSValue&);
  StyleImage* CachedOrPendingFromValue(CSSPropertyID, const CSSImageValue&);
  StyleImage* SetOrPendingFromValue(CSSPropertyID, const CSSImageSetValue&);
  SVGElementProxy& CachedOrPendingFromValue(const CSSURIValue&);

  void LoadPendingResources(ComputedStyle*);

 private:
  StyleImage* GeneratedOrPendingFromValue(CSSPropertyID,
                                          const CSSImageGeneratorValue&);

  void LoadPendingSVGDocuments(ComputedStyle*);
  void LoadPendingImages(ComputedStyle*);

  StyleImage* LoadPendingImage(
      ComputedStyle*,
      StylePendingImage*,
      FetchParameters::PlaceholderImageRequestType,
      CrossOriginAttributeValue = kCrossOriginAttributeNotSet);

  Member<Document> document_;
  HashSet<CSSPropertyID> pending_image_properties_;
  float device_scale_factor_;
};

}  // namespace blink

#endif  // ElementStyleResources_h
