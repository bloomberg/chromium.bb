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

#include "core/css/resolver/ElementStyleResources.h"

#include "core/CSSPropertyNames.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSURIValue.h"
#include "core/dom/Document.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ContentData.h"
#include "core/style/CursorData.h"
#include "core/style/FillLayer.h"
#include "core/style/FilterOperation.h"
#include "core/style/StyleFetchedImage.h"
#include "core/style/StyleFetchedImageSet.h"
#include "core/style/StyleGeneratedImage.h"
#include "core/style/StyleImage.h"
#include "core/style/StyleInvalidImage.h"
#include "core/style/StylePendingImage.h"
#include "core/svg/SVGElementProxy.h"
#include "platform/Length.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

ElementStyleResources::ElementStyleResources(Document& document,
                                             float device_scale_factor)
    : document_(&document), device_scale_factor_(device_scale_factor) {}

StyleImage* ElementStyleResources::GetStyleImage(CSSPropertyID property,
                                                 const CSSValue& value) {
  if (value.IsImageValue())
    return CachedOrPendingFromValue(property, ToCSSImageValue(value));

  if (value.IsImageGeneratorValue())
    return GeneratedOrPendingFromValue(property,
                                       ToCSSImageGeneratorValue(value));

  if (value.IsImageSetValue())
    return SetOrPendingFromValue(property, ToCSSImageSetValue(value));

  return nullptr;
}

StyleImage* ElementStyleResources::GeneratedOrPendingFromValue(
    CSSPropertyID property,
    const CSSImageGeneratorValue& value) {
  if (value.IsPending()) {
    pending_image_properties_.insert(property);
    return StylePendingImage::Create(value);
  }
  return StyleGeneratedImage::Create(value);
}

StyleImage* ElementStyleResources::SetOrPendingFromValue(
    CSSPropertyID property,
    const CSSImageSetValue& value) {
  if (value.IsCachePending(device_scale_factor_)) {
    pending_image_properties_.insert(property);
    return StylePendingImage::Create(value);
  }
  return value.CachedImage(device_scale_factor_);
}

StyleImage* ElementStyleResources::CachedOrPendingFromValue(
    CSSPropertyID property,
    const CSSImageValue& value) {
  if (value.IsCachePending()) {
    pending_image_properties_.insert(property);
    return StylePendingImage::Create(value);
  }
  value.RestoreCachedResourceIfNeeded(*document_);
  return value.CachedImage();
}

SVGElementProxy& ElementStyleResources::CachedOrPendingFromValue(
    const CSSURIValue& value) {
  return value.EnsureElementProxy(*document_);
}

void ElementStyleResources::LoadPendingSVGDocuments(
    ComputedStyle* computed_style) {
  if (!computed_style->HasFilter())
    return;
  FilterOperations::FilterOperationVector& filter_operations =
      computed_style->MutableFilter().Operations();
  for (auto& filter_operation : filter_operations) {
    if (filter_operation->GetType() != FilterOperation::REFERENCE)
      continue;
    ReferenceFilterOperation& reference_operation =
        ToReferenceFilterOperation(*filter_operation);
    reference_operation.ElementProxy().Resolve(*document_);
  }
}

static bool ComputedStyleMayBeCSSSpriteBackgroundImage(
    const ComputedStyle& style) {
  // Simple heuristic to guess if CSS background image is used to create CSS
  // sprites. For a legit background image it's very likely the X and the Y
  // position will not be explicitly specifed. For CSS sprite image,
  // background X or Y position will probably be specified.
  const FillLayer& background = style.BackgroundLayers();
  return style.HasBackgroundImage() &&
         (background.PositionX().IsFixed() || background.PositionY().IsFixed());
}

StyleImage* ElementStyleResources::LoadPendingImage(
    ComputedStyle* style,
    StylePendingImage* pending_image,
    FetchParameters::PlaceholderImageRequestType placeholder_image_request_type,
    CrossOriginAttributeValue cross_origin) {
  if (CSSImageValue* image_value = pending_image->CssImageValue()) {
    return image_value->CacheImage(*document_, placeholder_image_request_type,
                                   cross_origin);
  }

  if (CSSPaintValue* paint_value = pending_image->CssPaintValue()) {
    StyleGeneratedImage* image = StyleGeneratedImage::Create(*paint_value);
    style->AddPaintImage(image);
    return image;
  }

  if (CSSImageGeneratorValue* image_generator_value =
          pending_image->CssImageGeneratorValue()) {
    image_generator_value->LoadSubimages(*document_);
    return StyleGeneratedImage::Create(*image_generator_value);
  }

  if (CSSImageSetValue* image_set_value = pending_image->CssImageSetValue()) {
    return image_set_value->CacheImage(*document_, device_scale_factor_,
                                       placeholder_image_request_type,
                                       cross_origin);
  }

  NOTREACHED();
  return nullptr;
}

void ElementStyleResources::LoadPendingImages(ComputedStyle* style) {
  // We must loop over the properties and then look at the style to see if
  // a pending image exists, and only load that image. For example:
  //
  // <style>
  //    div { background-image: url(a.png); }
  //    div { background-image: url(b.png); }
  //    div { background-image: none; }
  // </style>
  // <div></div>
  //
  // We call styleImage() for both a.png and b.png adding the
  // CSSPropertyBackgroundImage property to the pending_image_properties_ set,
  // then we null out the background image because of the "none".
  //
  // If we eagerly loaded the images we'd fetch a.png, even though it's not
  // used. If we didn't null check below we'd crash since the none actually
  // removed all background images.

  for (CSSPropertyID property : pending_image_properties_) {
    switch (property) {
      case CSSPropertyBackgroundImage: {
        for (FillLayer* background_layer = &style->AccessBackgroundLayers();
             background_layer; background_layer = background_layer->Next()) {
          if (background_layer->GetImage() &&
              background_layer->GetImage()->IsPendingImage()) {
            background_layer->SetImage(LoadPendingImage(
                style, ToStylePendingImage(background_layer->GetImage()),
                ComputedStyleMayBeCSSSpriteBackgroundImage(*style)
                    ? FetchParameters::kDisallowPlaceholder
                    : FetchParameters::kAllowPlaceholder));
          }
        }
        break;
      }
      case CSSPropertyContent: {
        for (ContentData* content_data =
                 const_cast<ContentData*>(style->GetContentData());
             content_data; content_data = content_data->Next()) {
          if (content_data->IsImage()) {
            StyleImage* image = ToImageContentData(content_data)->GetImage();
            if (image->IsPendingImage()) {
              ToImageContentData(content_data)
                  ->SetImage(
                      LoadPendingImage(style, ToStylePendingImage(image),
                                       FetchParameters::kAllowPlaceholder));
            }
          }
        }
        break;
      }
      case CSSPropertyCursor: {
        if (CursorList* cursor_list = style->Cursors()) {
          for (size_t i = 0; i < cursor_list->size(); ++i) {
            CursorData& current_cursor = cursor_list->at(i);
            if (StyleImage* image = current_cursor.GetImage()) {
              if (image->IsPendingImage()) {
                // cursor images shouldn't be replaced with placeholders
                current_cursor.SetImage(
                    LoadPendingImage(style, ToStylePendingImage(image),
                                     FetchParameters::kDisallowPlaceholder));
              }
            }
          }
        }
        break;
      }
      case CSSPropertyListStyleImage: {
        if (style->ListStyleImage() &&
            style->ListStyleImage()->IsPendingImage()) {
          // List style images shouldn't be replaced with placeholders
          style->SetListStyleImage(LoadPendingImage(
              style, ToStylePendingImage(style->ListStyleImage()),
              FetchParameters::kDisallowPlaceholder));
        }
        break;
      }
      case CSSPropertyBorderImageSource: {
        if (style->BorderImageSource() &&
            style->BorderImageSource()->IsPendingImage()) {
          // Border images shouldn't be replaced with placeholders
          style->SetBorderImageSource(LoadPendingImage(
              style, ToStylePendingImage(style->BorderImageSource()),
              FetchParameters::kDisallowPlaceholder));
        }
        break;
      }
      case CSSPropertyWebkitBoxReflect: {
        if (StyleReflection* reflection = style->BoxReflect()) {
          const NinePieceImage& mask_image = reflection->Mask();
          if (mask_image.GetImage() &&
              mask_image.GetImage()->IsPendingImage()) {
            StyleImage* loaded_image = LoadPendingImage(
                style, ToStylePendingImage(mask_image.GetImage()),
                FetchParameters::kAllowPlaceholder);
            reflection->SetMask(NinePieceImage(
                loaded_image, mask_image.ImageSlices(), mask_image.Fill(),
                mask_image.BorderSlices(), mask_image.Outset(),
                mask_image.HorizontalRule(), mask_image.VerticalRule()));
          }
        }
        break;
      }
      case CSSPropertyWebkitMaskBoxImageSource: {
        if (style->MaskBoxImageSource() &&
            style->MaskBoxImageSource()->IsPendingImage()) {
          style->SetMaskBoxImageSource(LoadPendingImage(
              style, ToStylePendingImage(style->MaskBoxImageSource()),
              FetchParameters::kAllowPlaceholder));
        }
        break;
      }
      case CSSPropertyWebkitMaskImage: {
        for (FillLayer* mask_layer = &style->AccessMaskLayers(); mask_layer;
             mask_layer = mask_layer->Next()) {
          if (mask_layer->GetImage() &&
              mask_layer->GetImage()->IsPendingImage()) {
            mask_layer->SetImage(LoadPendingImage(
                style, ToStylePendingImage(mask_layer->GetImage()),
                FetchParameters::kAllowPlaceholder));
          }
        }
        break;
      }
      case CSSPropertyShapeOutside:
        if (style->ShapeOutside() && style->ShapeOutside()->GetImage() &&
            style->ShapeOutside()->GetImage()->IsPendingImage()) {
          style->ShapeOutside()->SetImage(LoadPendingImage(
              style, ToStylePendingImage(style->ShapeOutside()->GetImage()),
              FetchParameters::kAllowPlaceholder,
              kCrossOriginAttributeAnonymous));
        }
        break;
      default:
        NOTREACHED();
    }
  }
}

void ElementStyleResources::LoadPendingResources(
    ComputedStyle* computed_style) {
  LoadPendingImages(computed_style);
  LoadPendingSVGDocuments(computed_style);
}

}  // namespace blink
