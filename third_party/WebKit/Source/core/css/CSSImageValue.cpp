/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "core/css/CSSImageValue.h"

#include "core/css/CSSMarkup.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/style/StyleFetchedImage.h"
#include "core/style/StyleInvalidImage.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace blink {

CSSImageValue::CSSImageValue(const AtomicString& raw_value,
                             const KURL& url,
                             const Referrer& referrer,
                             StyleImage* image)
    : CSSValue(kImageClass),
      relative_url_(raw_value),
      referrer_(referrer),
      absolute_url_(url.GetString()),
      cached_image_(image) {}

CSSImageValue::CSSImageValue(const AtomicString& absolute_url)
    : CSSValue(kImageClass),
      relative_url_(absolute_url),
      absolute_url_(absolute_url) {}

CSSImageValue::~CSSImageValue() {}

StyleImage* CSSImageValue::CacheImage(
    const Document& document,
    FetchParameters::PlaceholderImageRequestType placeholder_image_request_type,
    CrossOriginAttributeValue cross_origin) {
  if (!cached_image_) {
    if (absolute_url_.IsEmpty())
      ReResolveURL(document);
    ResourceRequest resource_request(absolute_url_);
    resource_request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
        referrer_.referrer_policy, resource_request.Url(), referrer_.referrer));
    ResourceLoaderOptions options;
    options.initiator_info.name = initiator_name_.IsEmpty()
                                      ? FetchInitiatorTypeNames::css
                                      : initiator_name_;
    FetchParameters params(resource_request, options);

    if (cross_origin != kCrossOriginAttributeNotSet) {
      params.SetCrossOriginAccessControl(document.GetSecurityOrigin(),
                                         cross_origin);
    }

    if (document.GetFrame() &&
        placeholder_image_request_type == FetchParameters::kAllowPlaceholder)
      document.GetFrame()->MaybeAllowImagePlaceholder(params);

    if (ImageResourceContent* cached_image =
            ImageResourceContent::Fetch(params, document.Fetcher())) {
      cached_image_ =
          StyleFetchedImage::Create(cached_image, document, params.Url());
    } else {
      cached_image_ = StyleInvalidImage::Create(Url());
    }
  }

  return cached_image_.Get();
}

void CSSImageValue::RestoreCachedResourceIfNeeded(
    const Document& document) const {
  if (!cached_image_ || !document.Fetcher() || absolute_url_.IsNull())
    return;

  ImageResourceContent* resource = cached_image_->CachedImage();
  if (!resource)
    return;

  resource->EmulateLoadStartedForInspector(
      document.Fetcher(), KURL(kParsedURLString, absolute_url_),
      initiator_name_.IsEmpty() ? FetchInitiatorTypeNames::css
                                : initiator_name_);
}

bool CSSImageValue::HasFailedOrCanceledSubresources() const {
  if (!cached_image_)
    return false;
  if (ImageResourceContent* cached_resource = cached_image_->CachedImage())
    return cached_resource->LoadFailedOrCanceled();
  return true;
}

bool CSSImageValue::Equals(const CSSImageValue& other) const {
  if (absolute_url_.IsEmpty() && other.absolute_url_.IsEmpty())
    return relative_url_ == other.relative_url_;
  return absolute_url_ == other.absolute_url_;
}

String CSSImageValue::CustomCSSText() const {
  return SerializeURI(relative_url_);
}

bool CSSImageValue::KnownToBeOpaque(const Document& document,
                                    const ComputedStyle& style) const {
  return cached_image_ ? cached_image_->KnownToBeOpaque(document, style)
                       : false;
}

void CSSImageValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(cached_image_);
  CSSValue::TraceAfterDispatch(visitor);
}

void CSSImageValue::ReResolveURL(const Document& document) const {
  KURL url = document.CompleteURL(relative_url_);
  AtomicString url_string(url.GetString());
  if (url_string == absolute_url_)
    return;
  absolute_url_ = url_string;
  cached_image_.Clear();
}

}  // namespace blink
