// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"

#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_source.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace {

struct SupportedPrefixesStruct {
  const char* prefix;
  network::mojom::blink::CSPHashAlgorithm type;
};

}  // namespace

namespace blink {

namespace {

bool HasSourceMatchInList(
    const Vector<network::mojom::blink::CSPSourcePtr>& list,
    const String& self_protocol,
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) {
  for (const auto& source : list) {
    if (CSPSourceMatches(*source, self_protocol, url, redirect_status)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool CSPSourceListAllows(
    const network::mojom::blink::CSPSourceList& source_list,
    const network::mojom::blink::CSPSource& self_source,
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) {
  // Wildcards match network schemes ('http', 'https', 'ftp', 'ws', 'wss'), and
  // the scheme of the protected resource:
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression. Other
  // schemes, including custom schemes, must be explicitly listed in a source
  // list.
  if (source_list.allow_star) {
    if (url.ProtocolIsInHTTPFamily() || url.ProtocolIs("ftp") ||
        url.ProtocolIs("ws") || url.ProtocolIs("wss") ||
        (!url.Protocol().IsEmpty() &&
         EqualIgnoringASCIICase(url.Protocol(), self_source.scheme)))
      return true;

    return HasSourceMatchInList(source_list.sources, self_source.scheme, url,
                                redirect_status);
  }
  if (source_list.allow_self && CSPSourceMatchesAsSelf(self_source, url)) {
    return true;
  }

  return HasSourceMatchInList(source_list.sources, self_source.scheme, url,
                              redirect_status);
}

bool CSPSourceListAllowNonce(
    const network::mojom::blink::CSPSourceList& source_list,
    const String& nonce) {
  String nonce_stripped = nonce.StripWhiteSpace();
  return !nonce_stripped.IsNull() &&
         source_list.nonces.Contains(nonce_stripped);
}

bool CSPSourceListAllowHash(
    const network::mojom::blink::CSPSourceList& source_list,
    const network::mojom::blink::CSPHashSource& hash_value) {
  for (const network::mojom::blink::CSPHashSourcePtr& hash :
       source_list.hashes) {
    if (*hash == hash_value)
      return true;
  }
  return false;
}

bool CSPSourceListIsNone(
    const network::mojom::blink::CSPSourceList& source_list) {
  return !source_list.sources.size() && !source_list.allow_self &&
         !source_list.allow_star && !source_list.allow_inline &&
         !source_list.allow_unsafe_hashes && !source_list.allow_eval &&
         !source_list.allow_wasm_eval && !source_list.allow_wasm_unsafe_eval &&
         !source_list.allow_dynamic && !source_list.nonces.size() &&
         !source_list.hashes.size();
}

bool CSPSourceListIsSelf(
    const network::mojom::blink::CSPSourceList& source_list) {
  return source_list.allow_self && !source_list.sources.size() &&
         !source_list.allow_star && !source_list.allow_inline &&
         !source_list.allow_unsafe_hashes && !source_list.allow_eval &&
         !source_list.allow_wasm_eval && !source_list.allow_wasm_unsafe_eval &&
         !source_list.allow_dynamic && !source_list.nonces.size() &&
         !source_list.hashes.size();
}

bool CSPSourceListIsHashOrNoncePresent(
    const network::mojom::blink::CSPSourceList& source_list) {
  return !source_list.nonces.IsEmpty() || !source_list.hashes.IsEmpty();
}

bool CSPSourceListAllowsURLBasedMatching(
    const network::mojom::blink::CSPSourceList& source_list) {
  return !source_list.allow_dynamic &&
         (source_list.sources.size() || source_list.allow_star ||
          source_list.allow_self);
}

bool CSPSourceListAllowAllInline(
    CSPDirectiveName directive_type,
    const network::mojom::blink::CSPSourceList& source_list) {
  if (directive_type != CSPDirectiveName::DefaultSrc &&
      !ContentSecurityPolicy::IsScriptDirective(directive_type) &&
      !ContentSecurityPolicy::IsStyleDirective(directive_type)) {
    return false;
  }

  return source_list.allow_inline &&
         !CSPSourceListIsHashOrNoncePresent(source_list) &&
         (!ContentSecurityPolicy::IsScriptDirective(directive_type) ||
          !source_list.allow_dynamic);
}

}  // namespace blink
