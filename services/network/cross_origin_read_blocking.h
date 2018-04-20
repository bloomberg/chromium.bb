// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CROSS_ORIGIN_READ_BLOCKING_H_
#define SERVICES_NETWORK_CROSS_ORIGIN_READ_BLOCKING_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece_forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class URLRequest;
}

namespace network {

struct ResourceResponse;

// CrossOriginReadBlocking (CORB) implements response blocking
// policy for Site Isolation.  CORB will monitor network responses to a
// renderer and block illegal responses so that a compromised renderer cannot
// steal private information from other sites.  For more details see
// services/network/cross_origin_read_blocking_explainer.md

class COMPONENT_EXPORT(NETWORK_SERVICE) CrossOriginReadBlocking {
 public:
  enum class MimeType {
    // Note that these values are used in histograms, and must not change.
    kHtml = 0,
    kXml = 1,
    kJson = 2,
    kPlain = 3,
    kOthers = 4,

    kMax,
    kInvalidMimeType = kMax,
  };

  // An instance for tracking the state of analyzing a single response
  // and deciding whether CORB should block the response.
  class COMPONENT_EXPORT(NETWORK_SERVICE) ResponseAnalyzer {
   public:
    // Creates a ResponseAnalyzer for the |request|, |response| pair.  The
    // ResponseAnalyzer will decide whether |response| needs to be blocked.
    ResponseAnalyzer(const net::URLRequest& request,
                     const ResourceResponse& response);

    ~ResponseAnalyzer();

    bool should_allow_based_on_headers() {
      return should_block_based_on_headers_ == kAllow;
    }

    bool needs_sniffing() {
      return should_block_based_on_headers_ == kNeedToSniffMore;
    }

    const CrossOriginReadBlocking::MimeType& canonical_mime_type() {
      return canonical_mime_type_;
    }

   private:
    // Three conclusions are possible from looking at the headers:
    //   - Allow: response doesn't need to be blocked (e.g. if it is same-origin
    //     or has been allowed via CORS headers)
    //   - Block: response needs to be blocked (e.g. text/html + nosniff)
    //   - NeedMoreData: cannot decide yet - need to sniff more body first.
    enum BlockingDecision {
      kAllow,
      kBlock,
      kNeedToSniffMore,
    };
    BlockingDecision ShouldBlockBasedOnHeaders(
        const net::URLRequest& request,
        const ResourceResponse& response);

    // Outcome of ShouldBlockBasedOnHeaders recorder inside the Create method.
    BlockingDecision should_block_based_on_headers_;

    // Canonical MIME type detected by ShouldBlockBasedOnHeaders.  Used to
    // determine if blocking the response is needed, as well as which type of
    // sniffing to perform.
    CrossOriginReadBlocking::MimeType canonical_mime_type_ =
        CrossOriginReadBlocking::MimeType::kInvalidMimeType;

    DISALLOW_COPY_AND_ASSIGN(ResponseAnalyzer);
  };

  // Used to strip response headers if a decision to block has been made.
  static void SanitizeBlockedResponse(
      const scoped_refptr<network::ResourceResponse>& response);

  // Returns explicitly named headers from
  // https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name.
  //
  // Note that CORB doesn't block responses allowed through CORS - this means
  // that the list of allowed headers below doesn't have to consider header
  // names listed in the Access-Control-Expose-Headers header.
  static std::vector<std::string> GetCorsSafelistedHeadersForTesting();

  // Three conclusions are possible from sniffing a byte sequence:
  //  - No: meaning that the data definitively doesn't match the indicated type.
  //  - Yes: meaning that the data definitive does match the indicated type.
  //  - Maybe: meaning that if more bytes are appended to the stream, it's
  //    possible to get a Yes result. For example, if we are sniffing for a tag
  //    like "<html", a kMaybe result would occur if the data contains just
  //    "<ht".
  enum SniffingResult {
    kNo,
    kMaybe,
    kYes,
  };

  // Returns the representative mime type enum value of the mime type of
  // response. For example, this returns the same value for all text/xml mime
  // type families such as application/xml, application/rss+xml.
  static MimeType GetCanonicalMimeType(base::StringPiece mime_type);

  // Returns whether this scheme is a target of the cross-origin read blocking
  // (CORB) policy.  This returns true only for http://* and https://* urls.
  static bool IsBlockableScheme(const GURL& frame_origin);

  // Returns whether there's a valid CORS header for frame_origin.  This is
  // simliar to CrossOriginAccessControl::passesAccessControlCheck(), but we use
  // sites as our security domain, not origins.
  // TODO(dsjang): this must be improved to be more accurate to the actual CORS
  // specification. For now, this works conservatively, allowing XSDs that are
  // not allowed by actual CORS rules by ignoring 1) credentials and 2)
  // methods. Preflight requests don't matter here since they are not used to
  // decide whether to block a response or not on the client side.
  static bool IsValidCorsHeaderSet(const url::Origin& frame_origin,
                                   const std::string& access_control_origin);

  static SniffingResult SniffForHTML(base::StringPiece data);
  static SniffingResult SniffForXML(base::StringPiece data);
  static SniffingResult SniffForJSON(base::StringPiece data);

  // Sniff for patterns that indicate |data| only ought to be consumed by XHR()
  // or fetch(). This detects Javascript parser-breaker and particular JS
  // infinite-loop patterns, which are used conventionally as a defense against
  // JSON data exfiltration by means of a <script> tag.
  static SniffingResult SniffForFetchOnlyResource(base::StringPiece data);

 private:
  CrossOriginReadBlocking();  // Not instantiable.

  DISALLOW_COPY_AND_ASSIGN(CrossOriginReadBlocking);
};

inline std::ostream& operator<<(
    std::ostream& out,
    const CrossOriginReadBlocking::MimeType& value) {
  out << static_cast<int>(value);
  return out;
}

}  // namespace network

#endif  // SERVICES_NETWORK_CROSS_ORIGIN_READ_BLOCKING_H_
