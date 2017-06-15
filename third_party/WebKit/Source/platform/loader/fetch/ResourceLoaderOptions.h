/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceLoaderOptions_h
#define ResourceLoaderOptions_h

#include "platform/CrossThreadCopier.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Allocator.h"

namespace blink {

enum DataBufferingPolicy : uint8_t { kBufferData, kDoNotBufferData };

enum ContentSecurityPolicyDisposition : uint8_t {
  kCheckContentSecurityPolicy,
  kDoNotCheckContentSecurityPolicy
};

enum RequestInitiatorContext : uint8_t {
  kDocumentContext,
  kWorkerContext,
};

enum StoredCredentials : uint8_t {
  kAllowStoredCredentials,
  kDoNotAllowStoredCredentials
};

// APIs like XMLHttpRequest and EventSource let the user decide whether to send
// credentials, but they're always sent for same-origin requests. Additional
// information is needed to handle cross-origin redirects correctly.
enum CredentialRequest : uint8_t {
  kClientRequestedCredentials,
  kClientDidNotRequestCredentials
};

enum SynchronousPolicy : uint8_t {
  kRequestSynchronously,
  kRequestAsynchronously
};

// A resource fetch can be marked as being CORS enabled. The loader must perform
// an access check upon seeing the response.
enum CORSEnabled : uint8_t { kNotCORSEnabled, kIsCORSEnabled };

// Was the request generated from a "parser-inserted" element?
// https://html.spec.whatwg.org/multipage/scripting.html#parser-inserted
enum ParserDisposition : uint8_t { kParserInserted, kNotParserInserted };

enum CacheAwareLoadingEnabled : uint8_t {
  kNotCacheAwareLoadingEnabled,
  kIsCacheAwareLoadingEnabled
};

struct ResourceLoaderOptions {
  USING_FAST_MALLOC(ResourceLoaderOptions);

 public:
  ResourceLoaderOptions(StoredCredentials allow_credentials,
                        CredentialRequest credentials_requested)
      : data_buffering_policy(kBufferData),
        allow_credentials(allow_credentials),
        credentials_requested(credentials_requested),
        content_security_policy_option(kCheckContentSecurityPolicy),
        request_initiator_context(kDocumentContext),
        synchronous_policy(kRequestAsynchronously),
        cors_enabled(kNotCORSEnabled),
        parser_disposition(kParserInserted),
        cache_aware_loading_enabled(kNotCacheAwareLoadingEnabled) {}

  // Answers the question "can a separate request with these different options
  // be re-used" (e.g. preload request) The safe (but possibly slow) answer is
  // always false.
  bool CanReuseRequest(const ResourceLoaderOptions& other) const {
    // dataBufferingPolicy differences are believed to be safe for re-use.
    // FIXME: check allowCredentials.
    // FIXME: check credentialsRequested.
    // FIXME: check contentSecurityPolicyOption.
    // initiatorInfo is purely informational and should be benign for re-use.
    // requestInitiatorContext is benign (indicates document vs. worker)
    if (synchronous_policy != other.synchronous_policy)
      return false;
    return cors_enabled == other.cors_enabled;
    // securityOrigin has more complicated checks which callers are responsible
    // for.
  }

  FetchInitiatorInfo initiator_info;

  // When adding members, CrossThreadResourceLoaderOptionsData should be
  // updated.
  DataBufferingPolicy data_buffering_policy;

  // Whether HTTP credentials and cookies are sent with the request.
  StoredCredentials allow_credentials;

  // Whether the client (e.g. XHR) wanted credentials in the first place.
  CredentialRequest credentials_requested;

  ContentSecurityPolicyDisposition content_security_policy_option;
  RequestInitiatorContext request_initiator_context;
  SynchronousPolicy synchronous_policy;

  // If the resource is loaded out-of-origin, whether or not to use CORS.
  CORSEnabled cors_enabled;

  RefPtr<SecurityOrigin> security_origin;
  String content_security_policy_nonce;
  IntegrityMetadataSet integrity_metadata;
  ParserDisposition parser_disposition;
  CacheAwareLoadingEnabled cache_aware_loading_enabled;
};

// Encode AtomicString (in FetchInitiatorInfo) as String to cross threads.
struct CrossThreadResourceLoaderOptionsData {
  DISALLOW_NEW();
  explicit CrossThreadResourceLoaderOptionsData(
      const ResourceLoaderOptions& options)
      : data_buffering_policy(options.data_buffering_policy),
        allow_credentials(options.allow_credentials),
        credentials_requested(options.credentials_requested),
        content_security_policy_option(options.content_security_policy_option),
        initiator_info(options.initiator_info),
        request_initiator_context(options.request_initiator_context),
        synchronous_policy(options.synchronous_policy),
        cors_enabled(options.cors_enabled),
        security_origin(options.security_origin
                            ? options.security_origin->IsolatedCopy()
                            : nullptr),
        content_security_policy_nonce(options.content_security_policy_nonce),
        integrity_metadata(options.integrity_metadata),
        parser_disposition(options.parser_disposition),
        cache_aware_loading_enabled(options.cache_aware_loading_enabled) {}

  operator ResourceLoaderOptions() const {
    ResourceLoaderOptions options(allow_credentials, credentials_requested);
    options.data_buffering_policy = data_buffering_policy;
    options.content_security_policy_option = content_security_policy_option;
    options.initiator_info = initiator_info;
    options.request_initiator_context = request_initiator_context;
    options.synchronous_policy = synchronous_policy;
    options.cors_enabled = cors_enabled;
    options.security_origin = security_origin;
    options.content_security_policy_nonce = content_security_policy_nonce;
    options.integrity_metadata = integrity_metadata;
    options.parser_disposition = parser_disposition;
    options.cache_aware_loading_enabled = cache_aware_loading_enabled;
    return options;
  }

  DataBufferingPolicy data_buffering_policy;
  StoredCredentials allow_credentials;
  CredentialRequest credentials_requested;
  ContentSecurityPolicyDisposition content_security_policy_option;
  CrossThreadFetchInitiatorInfoData initiator_info;
  RequestInitiatorContext request_initiator_context;
  SynchronousPolicy synchronous_policy;
  CORSEnabled cors_enabled;
  RefPtr<SecurityOrigin> security_origin;
  String content_security_policy_nonce;
  IntegrityMetadataSet integrity_metadata;
  ParserDisposition parser_disposition;
  CacheAwareLoadingEnabled cache_aware_loading_enabled;
};

template <>
struct CrossThreadCopier<ResourceLoaderOptions> {
  using Type = CrossThreadResourceLoaderOptionsData;
  static Type Copy(const ResourceLoaderOptions& options) {
    return CrossThreadResourceLoaderOptionsData(options);
  }
};

}  // namespace blink

#endif  // ResourceLoaderOptions_h
