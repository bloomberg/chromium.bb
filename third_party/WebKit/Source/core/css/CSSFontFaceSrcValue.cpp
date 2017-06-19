/*
 * Copyright (C) 2007, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSFontFaceSrcValue.h"

#include "core/css/CSSMarkup.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/loader/resource/FontResource.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

bool CSSFontFaceSrcValue::IsSupportedFormat() const {
  // Normally we would just check the format, but in order to avoid conflicts
  // with the old WinIE style of font-face, we will also check to see if the URL
  // ends with .eot.  If so, we'll go ahead and assume that we shouldn't load
  // it.
  if (format_.IsEmpty()) {
    return absolute_resource_.StartsWithIgnoringASCIICase("data:") ||
           !absolute_resource_.EndsWithIgnoringASCIICase(".eot");
  }

  return FontCustomPlatformData::SupportsFormat(format_);
}

String CSSFontFaceSrcValue::CustomCSSText() const {
  StringBuilder result;
  if (IsLocal()) {
    result.Append("local(");
    result.Append(SerializeString(absolute_resource_));
    result.Append(')');
  } else {
    result.Append(SerializeURI(specified_resource_));
  }
  if (!format_.IsEmpty()) {
    result.Append(" format(");
    result.Append(SerializeString(format_));
    result.Append(')');
  }
  return result.ToString();
}

bool CSSFontFaceSrcValue::HasFailedOrCanceledSubresources() const {
  return fetched_ && fetched_->GetResource()->LoadFailedOrCanceled();
}

FontResource* CSSFontFaceSrcValue::Fetch(Document* document) const {
  if (!fetched_) {
    ResourceRequest resource_request(absolute_resource_);
    resource_request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
        referrer_.referrer_policy, resource_request.Url(), referrer_.referrer));
    ResourceLoaderOptions options(kAllowStoredCredentials,
                                  kClientRequestedCredentials);
    options.initiator_info.name = FetchInitiatorTypeNames::css;
    FetchParameters params(resource_request, options);
    if (RuntimeEnabledFeatures::WebFontsCacheAwareTimeoutAdaptationEnabled())
      params.SetCacheAwareLoadingEnabled(kIsCacheAwareLoadingEnabled);
    params.SetContentSecurityCheck(should_check_content_security_policy_);
    SecurityOrigin* security_origin = document->GetSecurityOrigin();

    // Local fonts are accessible from file: URLs even when
    // allowFileAccessFromFileURLs is false.
    if (!params.Url().IsLocalFile()) {
      params.SetCrossOriginAccessControl(security_origin,
                                         kCrossOriginAttributeAnonymous);
    }

    FontResource* resource = FontResource::Fetch(params, document->Fetcher());
    if (!resource)
      return nullptr;
    fetched_ = FontResourceHelper::Create(resource);
  } else {
    // FIXME: CSSFontFaceSrcValue::Fetch is invoked when @font-face rule
    // is processed by StyleResolver / StyleEngine.
    RestoreCachedResourceIfNeeded(document);
  }
  return fetched_->GetResource();
}

void CSSFontFaceSrcValue::RestoreCachedResourceIfNeeded(
    Document* document) const {
  DCHECK(fetched_);
  DCHECK(document);
  DCHECK(document->Fetcher());

  const String resource_url = document->CompleteURL(absolute_resource_);
  DCHECK_EQ(should_check_content_security_policy_,
            fetched_->GetResource()->Options().content_security_policy_option);
  document->Fetcher()->EmulateLoadStartedForInspector(
      fetched_->GetResource(), KURL(kParsedURLString, resource_url),
      WebURLRequest::kRequestContextFont, FetchInitiatorTypeNames::css);
}

bool CSSFontFaceSrcValue::Equals(const CSSFontFaceSrcValue& other) const {
  return is_local_ == other.is_local_ && format_ == other.format_ &&
         specified_resource_ == other.specified_resource_ &&
         absolute_resource_ == other.absolute_resource_;
}

}  // namespace blink
