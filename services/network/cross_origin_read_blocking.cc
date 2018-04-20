// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cross_origin_read_blocking.h"

#include <stddef.h>

#include <string>
#include <unordered_set>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/resource_response_info.h"

using base::StringPiece;
using MimeType = network::CrossOriginReadBlocking::MimeType;
using SniffingResult = network::CrossOriginReadBlocking::SniffingResult;

namespace network {

namespace {

// MIME types
const char kTextHtml[] = "text/html";
const char kTextXml[] = "text/xml";
const char kAppXml[] = "application/xml";
const char kAppJson[] = "application/json";
const char kImageSvg[] = "image/svg+xml";
const char kTextJson[] = "text/json";
const char kTextPlain[] = "text/plain";
// TODO(lukasza): Remove kJsonProtobuf once this MIME type is not used in
// practice.  See also https://crbug.com/826756#c3
const char kJsonProtobuf[] = "application/json+protobuf";

// MIME type suffixes
const char kJsonSuffix[] = "+json";
const char kXmlSuffix[] = "+xml";

void AdvancePastWhitespace(StringPiece* data) {
  size_t offset = data->find_first_not_of(" \t\r\n");
  if (offset == base::StringPiece::npos) {
    // |data| was entirely whitespace.
    data->clear();
  } else {
    data->remove_prefix(offset);
  }
}

// Returns kYes if |data| starts with one of the string patterns in
// |signatures|, kMaybe if |data| is a prefix of one of the patterns in
// |signatures|, and kNo otherwise.
//
// When kYes is returned, the matching prefix is erased from |data|.
SniffingResult MatchesSignature(StringPiece* data,
                                const StringPiece signatures[],
                                size_t arr_size,
                                base::CompareCase compare_case) {
  for (size_t i = 0; i < arr_size; ++i) {
    if (signatures[i].length() <= data->length()) {
      if (base::StartsWith(*data, signatures[i], compare_case)) {
        // When |signatures[i]| is a prefix of |data|, it constitutes a match.
        // Strip the matching characters, and return.
        data->remove_prefix(signatures[i].length());
        return CrossOriginReadBlocking::kYes;
      }
    } else {
      if (base::StartsWith(signatures[i], *data, compare_case)) {
        // When |data| is a prefix of |signatures[i]|, that means that
        // subsequent bytes in the stream could cause a match to occur.
        return CrossOriginReadBlocking::kMaybe;
      }
    }
  }
  return CrossOriginReadBlocking::kNo;
}

// Headers from
// https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name.
//
// Note that XSDB doesn't block responses allowed through CORS - this means
// that the list of allowed headers below doesn't have to consider header
// names listed in the Access-Control-Expose-Headers header.
const char* const kCorsSafelistedHeaders[] = {
    "cache-control", "content-language", "content-type",
    "expires",       "last-modified",    "pragma",
};

// Removes headers that should be blocked in cross-origin case.
// See https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name.
void BlockResponseHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  DCHECK(headers);
  std::unordered_set<std::string> names_of_headers_to_remove;

  size_t it = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&it, &name, &value)) {
    // Don't remove CORS headers - doing so would lead to incorrect error
    // messages for CORS-blocked responses (e.g. Blink would say "[...] No
    // 'Access-Control-Allow-Origin' header is present [...]" instead of saying
    // something like "[...] Access-Control-Allow-Origin' header has a value
    // 'http://www2.localhost:8000' that is not equal to the supplied origin
    // [...]").
    if (base::StartsWith(name, "Access-Control-",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }

    // Remove all other headers (but note the final exclusion below).
    names_of_headers_to_remove.insert(base::ToLowerASCII(name));
  }

  // Exclude from removals headers from
  // https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name.
  for (const char* header : kCorsSafelistedHeaders)
    names_of_headers_to_remove.erase(header);

  headers->RemoveHeaders(names_of_headers_to_remove);
}

}  // namespace

MimeType CrossOriginReadBlocking::GetCanonicalMimeType(
    base::StringPiece mime_type) {
  // Checking for image/svg+xml early ensures that it won't get classified as
  // MimeType::kXml by the presence of the "+xml"
  // suffix.
  if (base::LowerCaseEqualsASCII(mime_type, kImageSvg))
    return MimeType::kOthers;

  // See also https://mimesniff.spec.whatwg.org/#html-mime-type
  if (base::LowerCaseEqualsASCII(mime_type, kTextHtml))
    return MimeType::kHtml;

  // See also https://mimesniff.spec.whatwg.org/#json-mime-type
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  if (base::LowerCaseEqualsASCII(mime_type, kAppJson) ||
      base::LowerCaseEqualsASCII(mime_type, kTextJson) ||
      base::LowerCaseEqualsASCII(mime_type, kJsonProtobuf) ||
      base::EndsWith(mime_type, kJsonSuffix, kCaseInsensitive)) {
    return MimeType::kJson;
  }

  // See also https://mimesniff.spec.whatwg.org/#xml-mime-type
  if (base::LowerCaseEqualsASCII(mime_type, kAppXml) ||
      base::LowerCaseEqualsASCII(mime_type, kTextXml) ||
      base::EndsWith(mime_type, kXmlSuffix, kCaseInsensitive)) {
    return MimeType::kXml;
  }

  if (base::LowerCaseEqualsASCII(mime_type, kTextPlain))
    return MimeType::kPlain;

  return MimeType::kOthers;
}

bool CrossOriginReadBlocking::IsBlockableScheme(const GURL& url) {
  // We exclude ftp:// from here. FTP doesn't provide a Content-Type
  // header which our policy depends on, so we cannot protect any
  // response from FTP servers.
  return url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme);
}

bool CrossOriginReadBlocking::IsValidCorsHeaderSet(
    const url::Origin& frame_origin,
    const std::string& access_control_origin) {
  // Many websites are sending back "\"*\"" instead of "*". This is
  // non-standard practice, and not supported by Chrome. Refer to
  // CrossOriginAccessControl::passesAccessControlCheck().

  // Note that "null" offers no more protection than "*" because it matches any
  // unique origin, such as data URLs. Any origin can thus access it, so don't
  // bother trying to block this case.

  // TODO(dsjang): * is not allowed for the response from a request
  // with cookies. This allows for more than what the renderer will
  // eventually be able to receive, so we won't see illegal cross-site
  // documents allowed by this. We have to find a way to see if this
  // response is from a cookie-tagged request or not in the future.
  if (access_control_origin == "*" || access_control_origin == "null")
    return true;

  return frame_origin.IsSameOriginWith(
      url::Origin::Create(GURL(access_control_origin)));
}

// This function is a slight modification of |net::SniffForHTML|.
SniffingResult CrossOriginReadBlocking::SniffForHTML(StringPiece data) {
  // The content sniffers used by Chrome and Firefox are using "<!--" as one of
  // the HTML signatures, but it also appears in valid JavaScript, considered as
  // well-formed JS by the browser.  Since we do not want to block any JS, we
  // exclude it from our HTML signatures. This can weaken our CORB policy,
  // but we can break less websites.
  //
  // Note that <body> and <br> are not included below, since <b is a prefix of
  // them.
  //
  // TODO(dsjang): parameterize |net::SniffForHTML| with an option that decides
  // whether to include <!-- or not, so that we can remove this function.
  // TODO(dsjang): Once CrossOriginReadBlocking is moved into the browser
  // process, we should do single-thread checking here for the static
  // initializer.
  static const StringPiece kHtmlSignatures[] = {
      StringPiece("<!doctype html"),  // HTML5 spec
      StringPiece("<script"),         // HTML5 spec, Mozilla
      StringPiece("<html"),           // HTML5 spec, Mozilla
      StringPiece("<head"),           // HTML5 spec, Mozilla
      StringPiece("<iframe"),         // Mozilla
      StringPiece("<h1"),             // Mozilla
      StringPiece("<div"),            // Mozilla
      StringPiece("<font"),           // Mozilla
      StringPiece("<table"),          // Mozilla
      StringPiece("<a"),              // Mozilla
      StringPiece("<style"),          // Mozilla
      StringPiece("<title"),          // Mozilla
      StringPiece("<b"),              // Mozilla (note: subsumes <body>, <br>)
      StringPiece("<p")               // Mozilla
  };

  while (data.length() > 0) {
    AdvancePastWhitespace(&data);

    SniffingResult signature_match =
        MatchesSignature(&data, kHtmlSignatures, arraysize(kHtmlSignatures),
                         base::CompareCase::INSENSITIVE_ASCII);
    if (signature_match != kNo)
      return signature_match;

    // "<!--" (the HTML comment syntax) is a special case, since it's valid JS
    // as well. Skip over them.
    static const StringPiece kBeginCommentSignature[] = {"<!--"};
    SniffingResult comment_match = MatchesSignature(
        &data, kBeginCommentSignature, arraysize(kBeginCommentSignature),
        base::CompareCase::SENSITIVE);
    if (comment_match != kYes)
      return comment_match;

    // Look for an end comment.
    static const StringPiece kEndComment = "-->";
    size_t comment_end = data.find(kEndComment);
    if (comment_end == base::StringPiece::npos)
      return kMaybe;  // Hit end of data with open comment.
    data.remove_prefix(comment_end + kEndComment.length());
  }

  // All of |data| was consumed, without a clear determination.
  return kMaybe;
}

SniffingResult CrossOriginReadBlocking::SniffForXML(base::StringPiece data) {
  // TODO(dsjang): Once CrossOriginReadBlocking is moved into the browser
  // process, we should do single-thread checking here for the static
  // initializer.
  AdvancePastWhitespace(&data);
  static const StringPiece kXmlSignatures[] = {StringPiece("<?xml")};
  return MatchesSignature(&data, kXmlSignatures, arraysize(kXmlSignatures),
                          base::CompareCase::SENSITIVE);
}

SniffingResult CrossOriginReadBlocking::SniffForJSON(base::StringPiece data) {
  // Currently this function looks for an opening brace ('{'), followed by a
  // double-quoted string literal, followed by a colon. Importantly, such a
  // sequence is a Javascript syntax error: although the JSON object syntax is
  // exactly Javascript's object-initializer syntax, a Javascript object-
  // initializer expression is not valid as a standalone Javascript statement.
  //
  // TODO(nick): We have to come up with a better way to sniff JSON. The
  // following are known limitations of this function:
  // https://crbug.com/795470/ Support non-dictionary values (e.g. lists)
  enum {
    kStartState,
    kLeftBraceState,
    kLeftQuoteState,
    kEscapeState,
    kRightQuoteState,
  } state = kStartState;

  for (size_t i = 0; i < data.length(); ++i) {
    const char c = data[i];
    if (state != kLeftQuoteState && state != kEscapeState) {
      // Whitespace is ignored (outside of string literals)
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        continue;
    } else {
      // Inside string literals, control characters should result in rejection.
      if ((c >= 0 && c < 32) || c == 127)
        return kNo;
    }

    switch (state) {
      case kStartState:
        if (c == '{')
          state = kLeftBraceState;
        else
          return kNo;
        break;
      case kLeftBraceState:
        if (c == '"')
          state = kLeftQuoteState;
        else
          return kNo;
        break;
      case kLeftQuoteState:
        if (c == '"')
          state = kRightQuoteState;
        else if (c == '\\')
          state = kEscapeState;
        break;
      case kEscapeState:
        // Simplification: don't bother rejecting hex escapes.
        state = kLeftQuoteState;
        break;
      case kRightQuoteState:
        if (c == ':')
          return kYes;
        else
          return kNo;
        break;
    }
  }
  return kMaybe;
}

SniffingResult CrossOriginReadBlocking::SniffForFetchOnlyResource(
    base::StringPiece data) {
  // kScriptBreakingPrefixes contains prefixes that are conventionally used to
  // prevent a JSON response from becoming a valid Javascript program (an attack
  // vector known as XSSI). The presence of such a prefix is a strong signal
  // that the resource is meant to be consumed only by the fetch API or
  // XMLHttpRequest, and is meant to be protected from use in non-CORS, cross-
  // origin contexts like <script>, <img>, etc.
  //
  // These prefixes work either by inducing a syntax error, or inducing an
  // infinite loop. In either case, the prefix must create a guarantee that no
  // matter what bytes follow it, the entire response would be worthless to
  // execute as a <script>.
  static const StringPiece kScriptBreakingPrefixes[] = {
      // Parser breaker prefix.
      //
      // Built into angular.js (followed by a comma and a newline):
      //   https://docs.angularjs.org/api/ng/service/$http
      //
      // Built into the Java Spring framework (followed by a comma and a space):
      //   https://goo.gl/xP7FWn
      //
      // Observed on google.com (without a comma, followed by a newline).
      StringPiece(")]}'"),

      // Apache struts: https://struts.apache.org/plugins/json/#prefix
      StringPiece("{}&&"),

      // Spring framework (historically): https://goo.gl/JYPFAv
      StringPiece("{} &&"),

      // Infinite loops.
      StringPiece("for(;;);"),  // observed on facebook.com
      StringPiece("while(1);"), StringPiece("for (;;);"),
      StringPiece("while (1);"),
  };
  SniffingResult has_parser_breaker = MatchesSignature(
      &data, kScriptBreakingPrefixes, arraysize(kScriptBreakingPrefixes),
      base::CompareCase::SENSITIVE);
  if (has_parser_breaker != kNo)
    return has_parser_breaker;

  // A non-empty JSON object also effectively introduces a JS syntax error.
  return SniffForJSON(data);
}

// static
void CrossOriginReadBlocking::SanitizeBlockedResponse(
    const scoped_refptr<network::ResourceResponse>& response) {
  DCHECK(response);
  response->head.content_length = 0;
  if (response->head.headers)
    BlockResponseHeaders(response->head.headers);
}

// static
std::vector<std::string>
CrossOriginReadBlocking::GetCorsSafelistedHeadersForTesting() {
  return std::vector<std::string>(
      kCorsSafelistedHeaders,
      kCorsSafelistedHeaders + arraysize(kCorsSafelistedHeaders));
}

CrossOriginReadBlocking::ResponseAnalyzer::ResponseAnalyzer(
    const net::URLRequest& request,
    const ResourceResponse& response) {
  should_block_based_on_headers_ = ShouldBlockBasedOnHeaders(request, response);
}

CrossOriginReadBlocking::ResponseAnalyzer::~ResponseAnalyzer() = default;

CrossOriginReadBlocking::ResponseAnalyzer::BlockingDecision
CrossOriginReadBlocking::ResponseAnalyzer::ShouldBlockBasedOnHeaders(
    const net::URLRequest& request,
    const ResourceResponse& response) {
  // The checks in this method are ordered to rule out blocking in most cases as
  // quickly as possible.  Checks that are likely to lead to returning false or
  // that are inexpensive should be near the top.
  url::Origin target_origin = url::Origin::Create(request.url());

  // Treat a missing initiator as an empty origin to be safe, though we don't
  // expect this to happen.  Unfortunately, this requires a copy.
  url::Origin initiator;
  if (request.initiator().has_value())
    initiator = request.initiator().value();

  // Don't block same-origin documents.
  if (initiator.IsSameOriginWith(target_origin))
    return kAllow;

  // Only block documents from HTTP(S) schemes.  Checking the scheme of
  // |target_origin| ensures that we also protect content of blob: and
  // filesystem: URLs if their nested origins have a HTTP(S) scheme.
  if (!IsBlockableScheme(target_origin.GetURL()))
    return kAllow;

  // Allow requests from file:// URLs for now.
  // TODO(creis): Limit this to when the allow_universal_access_from_file_urls
  // preference is set.  See https://crbug.com/789781.
  if (initiator.scheme() == url::kFileScheme)
    return kAllow;

  // Allow the response through if it has valid CORS headers.
  std::string cors_header;
  response.head.headers->GetNormalizedHeader("access-control-allow-origin",
                                             &cors_header);
  if (IsValidCorsHeaderSet(initiator, cors_header)) {
    return kAllow;
  }

  // Requests from foo.example.com will consult foo.example.com's service worker
  // first (if one has been registered).  The service worker can handle requests
  // initiated by foo.example.com even if they are cross-origin (e.g. requests
  // for bar.example.com).  This is okay and should not be blocked by CORB,
  // unless the initiator opted out of CORS / opted into receiving an opaque
  // response.  See also https://crbug.com/803672.
  if (response.head.was_fetched_via_service_worker) {
    switch (response.head.response_type_via_service_worker) {
      case network::mojom::FetchResponseType::kBasic:
      case network::mojom::FetchResponseType::kCORS:
      case network::mojom::FetchResponseType::kDefault:
      case network::mojom::FetchResponseType::kError:
        // Non-opaque responses shouldn't be blocked.
        return kAllow;
      case network::mojom::FetchResponseType::kOpaque:
      case network::mojom::FetchResponseType::kOpaqueRedirect:
        // Opaque responses are eligible for blocking. Continue on...
        break;
    }
  }

  // We intend to block the response at this point.  However, we will usually
  // sniff the contents to confirm the MIME type, to avoid blocking incorrectly
  // labeled JavaScript, JSONP, etc files.
  //
  // Note: if there is a nosniff header, it means we should honor the response
  // mime type without trying to confirm it.
  std::string nosniff_header;
  response.head.headers->GetNormalizedHeader("x-content-type-options",
                                             &nosniff_header);
  bool has_nosniff_header =
      base::LowerCaseEqualsASCII(nosniff_header, "nosniff");

  // CORB should look directly at the Content-Type header if one has been
  // received from the network.  Ignoring |response.head.mime_type| helps avoid
  // breaking legitimate websites (which might happen more often when blocking
  // would be based on the mime type sniffed by MimeSniffingResourceHandler).
  //
  // TODO(nick): What if the mime type is omitted? Should that be treated the
  // same as text/plain? https://crbug.com/795971
  std::string mime_type;
  if (response.head.headers)
    response.head.headers->GetMimeType(&mime_type);
  // Canonicalize the MIME type.  Note that even if it doesn't claim to be a
  // blockable type (i.e., HTML, XML, JSON, or plain text), it may still fail
  // the checks during the SniffForFetchOnlyResource() phase.
  canonical_mime_type_ =
      network::CrossOriginReadBlocking::GetCanonicalMimeType(mime_type);

  // If this is a partial response, sniffing is not possible, so allow the
  // response if it's not a protected mime type.
  std::string range_header;
  response.head.headers->GetNormalizedHeader("content-range", &range_header);
  bool has_range_header = !range_header.empty();
  if (has_range_header) {
    switch (canonical_mime_type_) {
      case MimeType::kOthers:
      case MimeType::kPlain:  // See also https://crbug.com/801709
        return kAllow;
      case MimeType::kHtml:
      case MimeType::kJson:
      case MimeType::kXml:
        return kBlock;
      case MimeType::kMax:
        NOTREACHED();
        return kBlock;
    }
  }

  // Decide whether to block based on the MIME type.
  switch (canonical_mime_type_) {
    case MimeType::kHtml:
    case MimeType::kXml:
    case MimeType::kJson:
    case MimeType::kPlain:
      if (has_nosniff_header)
        return kBlock;
      else
        return kNeedToSniffMore;
      break;

    case MimeType::kOthers:
      // Stylesheets shouldn't be sniffed for JSON parser breakers - see
      // https://crbug.com/809259.
      if (base::LowerCaseEqualsASCII(response.head.mime_type, "text/css"))
        return kAllow;
      else
        return kNeedToSniffMore;
      break;

    case MimeType::kInvalidMimeType:
      NOTREACHED();
      return kBlock;
  }
  NOTREACHED();
  return kBlock;
}

}  // namespace network
