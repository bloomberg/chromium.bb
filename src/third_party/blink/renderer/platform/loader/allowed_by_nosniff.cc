// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/allowed_by_nosniff.h"

#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

using WebFeature = mojom::WebFeature;

// In addition to makeing an allowed/not-allowed decision,
// AllowedByNosniff::MimeTypeAsScript reports common usage patterns to support
// future decisions about which types can be safely be disallowed. Below
// is a number of constants about which use counters to report.

const WebFeature kApplicationFeatures[2] = {
    WebFeature::kCrossOriginApplicationScript,
    WebFeature::kSameOriginApplicationScript};

const WebFeature kTextFeatures[2] = {WebFeature::kCrossOriginTextScript,
                                     WebFeature::kSameOriginTextScript};

const WebFeature kApplicationOctetStreamFeatures[2][2] = {
    {WebFeature::kCrossOriginApplicationOctetStream,
     WebFeature::kCrossOriginWorkerApplicationOctetStream},
    {WebFeature::kSameOriginApplicationOctetStream,
     WebFeature::kSameOriginWorkerApplicationOctetStream}};

const WebFeature kApplicationXmlFeatures[2][2] = {
    {WebFeature::kCrossOriginApplicationXml,
     WebFeature::kCrossOriginWorkerApplicationXml},
    {WebFeature::kSameOriginApplicationXml,
     WebFeature::kSameOriginWorkerApplicationXml}};

const WebFeature kTextHtmlFeatures[2][2] = {
    {WebFeature::kCrossOriginTextHtml, WebFeature::kCrossOriginWorkerTextHtml},
    {WebFeature::kSameOriginTextHtml, WebFeature::kSameOriginWorkerTextHtml}};

const WebFeature kTextPlainFeatures[2][2] = {
    {WebFeature::kCrossOriginTextPlain,
     WebFeature::kCrossOriginWorkerTextPlain},
    {WebFeature::kSameOriginTextPlain, WebFeature::kSameOriginWorkerTextPlain}};

const WebFeature kTextXmlFeatures[2][2] = {
    {WebFeature::kCrossOriginTextXml, WebFeature::kCrossOriginWorkerTextXml},
    {WebFeature::kSameOriginTextXml, WebFeature::kSameOriginWorkerTextXml}};

// Helper function to decide what to do with with a given mime type. This takes
// - a mime type
// - inputs that affect the decision (is_same_origin, mime_type_check_mode).
//
// The return value determines whether this mime should be allowed or blocked.
// Additionally, warn returns whether we should log a console warning about
// expected future blocking of this resource. 'counter' determines which
// Use counter should be used to count this. 'is_worker_global_scope' is used
// for choosing 'counter' value.
bool AllowMimeTypeAsScript(const String& mime_type,
                           bool same_origin,
                           AllowedByNosniff::MimeTypeCheck mime_type_check_mode,
                           bool is_worker_global_scope,
                           bool& warn,
                           WebFeature& counter) {
  using MimeTypeCheck = AllowedByNosniff::MimeTypeCheck;

  // The common case: A proper JavaScript MIME type
  if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type))
    return true;

  // Check for certain non-executable MIME types.
  // See:
  // https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-mime-type
  if (mime_type.StartsWithIgnoringASCIICase("image/") ||
      mime_type.StartsWithIgnoringASCIICase("text/csv") ||
      mime_type.StartsWithIgnoringASCIICase("audio/") ||
      mime_type.StartsWithIgnoringASCIICase("video/")) {
    if (mime_type.StartsWithIgnoringASCIICase("image/")) {
      counter = WebFeature::kBlockedSniffingImageToScript;
    } else if (mime_type.StartsWithIgnoringASCIICase("audio/")) {
      counter = WebFeature::kBlockedSniffingAudioToScript;
    } else if (mime_type.StartsWithIgnoringASCIICase("video/")) {
      counter = WebFeature::kBlockedSniffingVideoToScript;
    } else if (mime_type.StartsWithIgnoringASCIICase("text/csv")) {
      counter = WebFeature::kBlockedSniffingCSVToScript;
    }
    return false;
  }

  // Beyond this point we handle legacy MIME types, where it depends whether
  // we still wish to accept them (or log them using UseCounter, or add a
  // deprecation warning to the console).

  if (mime_type_check_mode == MimeTypeCheck::kLax &&
      mime_type.StartsWithIgnoringASCIICase("text/") &&
      MIMETypeRegistry::IsLegacySupportedJavaScriptLanguage(
          mime_type.Substring(5))) {
    return true;
  }

  if (mime_type.StartsWithIgnoringASCIICase("application/octet-stream")) {
    counter =
        kApplicationOctetStreamFeatures[same_origin][is_worker_global_scope];
  } else if (mime_type.StartsWithIgnoringASCIICase("application/xml")) {
    counter = kApplicationXmlFeatures[same_origin][is_worker_global_scope];
  } else if (mime_type.StartsWithIgnoringASCIICase("text/html")) {
    counter = kTextHtmlFeatures[same_origin][is_worker_global_scope];
  } else if (mime_type.StartsWithIgnoringASCIICase("text/plain")) {
    counter = kTextPlainFeatures[same_origin][is_worker_global_scope];
  } else if (mime_type.StartsWithIgnoringCase("text/xml")) {
    counter = kTextXmlFeatures[same_origin][is_worker_global_scope];
  }

  // Depending on RuntimeEnabledFeatures, we'll allow, allow-but-warn, or block
  // these types when we're in a worker.
  bool allow = mime_type_check_mode == MimeTypeCheck::kLax ||
               !RuntimeEnabledFeatures::WorkerNosniffBlockEnabled();
  warn = allow && mime_type_check_mode == MimeTypeCheck::kStrict &&
         RuntimeEnabledFeatures::WorkerNosniffWarnEnabled();
  return allow;
}

}  // namespace

bool AllowedByNosniff::MimeTypeAsScript(FetchContext& context,
                                        ConsoleLogger* console_logger,
                                        const ResourceResponse& response,
                                        MimeTypeCheck mime_type_check_mode,
                                        bool is_worker_global_scope) {
  // The content type is really only meaningful for the http:-family & data
  // schemes.
  bool is_http_family_or_data =
      response.CurrentRequestUrl().ProtocolIsInHTTPFamily() ||
      response.CurrentRequestUrl().ProtocolIsData();
  if (!is_http_family_or_data &&
      (response.CurrentRequestUrl().LastPathComponent().EndsWith(".js") ||
       response.CurrentRequestUrl().LastPathComponent().EndsWith(".mjs"))) {
    return true;
  }

  String mime_type = response.HttpContentType();

  // Allowed by nosniff?
  if (!(ParseContentTypeOptionsHeader(response.HttpHeaderField(
            http_names::kXContentTypeOptions)) != kContentTypeOptionsNosniff ||
        MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type))) {
    console_logger->AddConsoleMessage(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Refused to execute script from '" +
            response.CurrentRequestUrl().ElidedString() +
            "' because its MIME type ('" + mime_type +
            "') is not executable, and strict MIME type checking is enabled.");
    return false;
  }

  // Check for certain non-executable MIME types.
  // See:
  // https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-mime-type
  bool same_origin = context.GetResourceFetcherProperties()
                         .GetFetchClientSettingsObject()
                         .GetSecurityOrigin()
                         ->CanRequest(response.CurrentRequestUrl());

  // For any MIME type, we can do three things: accept/reject it, print a
  // warning into the console, and count it using a use counter.
  const WebFeature kWebFeatureNone = WebFeature::kNumberOfFeatures;
  bool warn = false;
  WebFeature counter = kWebFeatureNone;
  bool allow =
      AllowMimeTypeAsScript(mime_type, same_origin, mime_type_check_mode,
                            is_worker_global_scope, warn, counter);

  // These record usages for two MIME types (without subtypes), per same/cross
  // origin.
  if (mime_type.StartsWithIgnoringASCIICase("application/")) {
    context.CountUsage(kApplicationFeatures[same_origin]);
  } else if (mime_type.StartsWithIgnoringASCIICase("text/")) {
    context.CountUsage(kTextFeatures[same_origin]);
  }

  // The code above has made a decision and handed down the result in accept,
  // warn, and counter.
  if (counter != kWebFeatureNone) {
    context.CountUsage(counter);
  }
  if (!allow || warn) {
    const char* msg =
        allow ? "Deprecated: Future versions will refuse" : "Refused";
    console_logger->AddConsoleMessage(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        String() + msg + " to execute script from '" +
            response.CurrentRequestUrl().ElidedString() +
            "' because its MIME type ('" + mime_type + "') is not executable.");
  }
  return allow;
}

}  // namespace blink
