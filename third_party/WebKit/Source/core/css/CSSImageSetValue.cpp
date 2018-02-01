/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSImageSetValue.h"

#include <algorithm>
#include "core/css/CSSImageValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/style/StyleFetchedImageSet.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

CSSImageSetValue::CSSImageSetValue(CSSParserMode parser_mode)
    : CSSValueList(kImageSetClass, kCommaSeparator),
      cached_scale_factor_(1),
      parser_mode_(parser_mode) {}

CSSImageSetValue::~CSSImageSetValue() = default;

void CSSImageSetValue::FillImageSet() {
  size_t length = this->length();
  size_t i = 0;
  while (i < length) {
    const CSSImageValue& image_value = ToCSSImageValue(Item(i));
    String image_url = image_value.Url();

    ++i;
    SECURITY_DCHECK(i < length);
    const CSSValue& scale_factor_value = Item(i);
    float scale_factor =
        ToCSSPrimitiveValue(scale_factor_value).GetFloatValue();

    ImageWithScale image;
    image.image_url = image_url;
    image.referrer = SecurityPolicy::GenerateReferrer(
        image_value.GetReferrer().referrer_policy, KURL(image_url),
        image_value.GetReferrer().referrer);
    image.scale_factor = scale_factor;
    images_in_set_.push_back(image);
    ++i;
  }

  // Sort the images so that they are stored in order from lowest resolution to
  // highest.
  std::sort(images_in_set_.begin(), images_in_set_.end(),
            CSSImageSetValue::CompareByScaleFactor);
}

CSSImageSetValue::ImageWithScale CSSImageSetValue::BestImageForScaleFactor(
    float scale_factor) {
  ImageWithScale image;
  size_t number_of_images = images_in_set_.size();
  for (size_t i = 0; i < number_of_images; ++i) {
    image = images_in_set_.at(i);
    if (image.scale_factor >= scale_factor)
      return image;
  }
  return image;
}

bool CSSImageSetValue::IsCachePending(float device_scale_factor) const {
  return !cached_image_ || device_scale_factor != cached_scale_factor_;
}

StyleImage* CSSImageSetValue::CachedImage(float device_scale_factor) const {
  DCHECK(!IsCachePending(device_scale_factor));
  return cached_image_.Get();
}

StyleImage* CSSImageSetValue::CacheImage(
    const Document& document,
    float device_scale_factor,
    FetchParameters::PlaceholderImageRequestType placeholder_image_request_type,
    CrossOriginAttributeValue cross_origin) {
  if (!images_in_set_.size())
    FillImageSet();

  if (IsCachePending(device_scale_factor)) {
    // FIXME: In the future, we want to take much more than deviceScaleFactor
    // into acount here. All forms of scale should be included:
    // Page::PageScaleFactor(), LocalFrame::PageZoomFactor(), and any CSS
    // transforms. https://bugs.webkit.org/show_bug.cgi?id=81698
    ImageWithScale image = BestImageForScaleFactor(device_scale_factor);
    ResourceRequest resource_request(document.CompleteURL(image.image_url));
    resource_request.SetHTTPReferrer(image.referrer);
    ResourceLoaderOptions options;
    options.initiator_info.name = parser_mode_ == kUASheetMode
                                      ? FetchInitiatorTypeNames::uacss
                                      : FetchInitiatorTypeNames::css;
    FetchParameters params(resource_request, options);

    if (cross_origin != kCrossOriginAttributeNotSet) {
      params.SetCrossOriginAccessControl(document.GetSecurityOrigin(),
                                         cross_origin);
    }

    if (document.GetFrame() &&
        placeholder_image_request_type == FetchParameters::kAllowPlaceholder)
      document.GetFrame()->MaybeAllowImagePlaceholder(params);

    cached_image_ = StyleFetchedImageSet::Create(
        ImageResourceContent::Fetch(params, document.Fetcher()),
        image.scale_factor, this, params.Url());
    cached_scale_factor_ = device_scale_factor;
  }

  return cached_image_.Get();
}

String CSSImageSetValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("-webkit-image-set(");

  size_t length = this->length();
  size_t i = 0;
  while (i < length) {
    if (i > 0)
      result.Append(", ");

    const CSSValue& image_value = Item(i);
    result.Append(image_value.CssText());
    result.Append(' ');

    ++i;
    SECURITY_DCHECK(i < length);
    const CSSValue& scale_factor_value = Item(i);
    result.Append(scale_factor_value.CssText());
    // FIXME: Eventually the scale factor should contain it's own unit
    // http://wkb.ug/100120.
    // For now 'x' is hard-coded in the parser, so we hard-code it here too.
    result.Append('x');

    ++i;
  }

  result.Append(')');
  return result.ToString();
}

bool CSSImageSetValue::HasFailedOrCanceledSubresources() const {
  if (!cached_image_)
    return false;
  if (ImageResourceContent* cached_resource = cached_image_->CachedImage())
    return cached_resource->LoadFailedOrCanceled();
  return true;
}

void CSSImageSetValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(cached_image_);
  CSSValueList::TraceAfterDispatch(visitor);
}

CSSImageSetValue* CSSImageSetValue::ValueWithURLsMadeAbsolute() {
  CSSImageSetValue* value = CSSImageSetValue::Create(parser_mode_);
  for (auto& item : *this)
    item->IsImageValue()
        ? value->Append(*ToCSSImageValue(*item).ValueWithURLMadeAbsolute())
        : value->Append(*item);
  return value;
}

}  // namespace blink
