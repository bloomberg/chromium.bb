/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#ifndef FetchParameters_h
#define FetchParameters_h

#include "platform/CrossOriginAttributeValue.h"
#include "platform/PlatformExport.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {
class SecurityOrigin;

// A FetchParameters is a "parameter object" for
// ResourceFetcher::requestResource to avoid the method having too many
// arguments.
class PLATFORM_EXPORT FetchParameters {
  STACK_ALLOCATED();

 public:
  enum DeferOption { kNoDefer, kLazyLoad, kIdleLoad };
  enum class SpeculativePreloadType {
    kNotSpeculative,
    kInDocument,  // The request was discovered in the main document
    kInserted     // The request was discovered in a document.write()
  };
  enum OriginRestriction {
    kUseDefaultOriginRestrictionForType,
    kRestrictToSameOrigin,
    kNoOriginRestriction
  };
  enum PlaceholderImageRequestType {
    kDisallowPlaceholder = 0,  // The requested image must not be a placeholder.
    kAllowPlaceholder,         // The image is allowed to be a placeholder.
  };
  // TODO(toyoshim): Consider to define an enum for preload options, and use it
  // instead of bool in this class, FrameFetchContext, and so on. If it is
  // reasonable, we try merging m_speculativePreload and m_linkPreload into one
  // enum type. See https://crbug.com/675883.

  struct ResourceWidth {
    DISALLOW_NEW();
    float width;
    bool is_set;

    ResourceWidth() : width(0), is_set(false) {}
  };

  FetchParameters(const ResourceRequest&,
                  const AtomicString& initiator,
                  const String& charset = String());
  FetchParameters(const ResourceRequest&,
                  const AtomicString& initiator,
                  const ResourceLoaderOptions&);
  FetchParameters(const ResourceRequest&, const FetchInitiatorInfo&);
  ~FetchParameters();

  ResourceRequest& MutableResourceRequest() { return resource_request_; }
  const ResourceRequest& GetResourceRequest() const {
    return resource_request_;
  }
  const KURL& Url() const { return resource_request_.Url(); }

  void SetRequestContext(WebURLRequest::RequestContext context) {
    resource_request_.SetRequestContext(context);
  }

  const String& Charset() const { return charset_; }
  void SetCharset(const String& charset) { charset_ = charset; }

  const ResourceLoaderOptions& Options() const { return options_; }

  DeferOption Defer() const { return defer_; }
  void SetDefer(DeferOption defer) { defer_ = defer; }

  ResourceWidth GetResourceWidth() const { return resource_width_; }
  void SetResourceWidth(ResourceWidth);

  ClientHintsPreferences& GetClientHintsPreferences() {
    return client_hint_preferences_;
  }

  bool IsSpeculativePreload() const {
    return speculative_preload_type_ != SpeculativePreloadType::kNotSpeculative;
  }
  SpeculativePreloadType GetSpeculativePreloadType() const {
    return speculative_preload_type_;
  }
  void SetSpeculativePreloadType(SpeculativePreloadType,
                                 double discovery_time = 0);

  double PreloadDiscoveryTime() { return preload_discovery_time_; }

  bool IsLinkPreload() const { return options_.initiator_info.is_link_preload; }
  void SetLinkPreload(bool is_link_preload) {
    options_.initiator_info.is_link_preload = is_link_preload;
  }

  void SetContentSecurityCheck(
      ContentSecurityPolicyDisposition content_security_policy_option) {
    options_.content_security_policy_option = content_security_policy_option;
  }
  void SetCrossOriginAccessControl(SecurityOrigin*, CrossOriginAttributeValue);
  OriginRestriction GetOriginRestriction() const { return origin_restriction_; }
  void SetOriginRestriction(OriginRestriction restriction) {
    origin_restriction_ = restriction;
  }
  const IntegrityMetadataSet IntegrityMetadata() const {
    return options_.integrity_metadata;
  }
  void SetIntegrityMetadata(const IntegrityMetadataSet& metadata) {
    options_.integrity_metadata = metadata;
  }

  String ContentSecurityPolicyNonce() const {
    return options_.content_security_policy_nonce;
  }
  void SetContentSecurityPolicyNonce(const String& nonce) {
    options_.content_security_policy_nonce = nonce;
  }

  void SetParserDisposition(ParserDisposition parser_disposition) {
    options_.parser_disposition = parser_disposition;
  }

  void SetCacheAwareLoadingEnabled(
      CacheAwareLoadingEnabled cache_aware_loading_enabled) {
    options_.cache_aware_loading_enabled = cache_aware_loading_enabled;
  }

  void MakeSynchronous();

  PlaceholderImageRequestType GetPlaceholderImageRequestType() const {
    return placeholder_image_request_type_;
  }

  // Configures the request to load an image placeholder if the request is
  // eligible (e.g. the url's protocol is HTTP, etc.). If this request is
  // non-eligible, this method doesn't modify the ResourceRequest. Calling this
  // method sets m_placeholderImageRequestType to the appropriate value.
  void SetAllowImagePlaceholder();

 private:
  ResourceRequest resource_request_;
  String charset_;
  ResourceLoaderOptions options_;
  SpeculativePreloadType speculative_preload_type_;
  double preload_discovery_time_;
  DeferOption defer_;
  OriginRestriction origin_restriction_;
  ResourceWidth resource_width_;
  ClientHintsPreferences client_hint_preferences_;
  PlaceholderImageRequestType placeholder_image_request_type_;
};

}  // namespace blink

#endif
