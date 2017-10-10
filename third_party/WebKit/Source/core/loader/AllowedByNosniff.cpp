// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/AllowedByNosniff.h"

#include "core/dom/ExecutionContext.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/http_names.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

namespace {

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

}  // namespace

bool AllowedByNosniff::MimeTypeAsScript(ExecutionContext* execution_context,
                                        const ResourceResponse& response) {
  String mime_type = response.HttpContentType();

  // Allowed by nosniff?
  if (!(ParseContentTypeOptionsHeader(response.HttpHeaderField(
            HTTPNames::X_Content_Type_Options)) != kContentTypeOptionsNosniff ||
        MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type))) {
    execution_context->AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel,
        "Refused to execute script from '" + response.Url().ElidedString() +
            "' because its MIME type ('" + mime_type +
            "') is not executable, and "
            "strict MIME type checking is "
            "enabled."));
    return false;
  }

  // TODO(mkwst): Remove the UseCoutner measurements below, once we verify
  // that other vendors are following along with these restrictions..

  // Check for certain non-executable MIME types.
  // See:
  // https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-mime-type
  if (mime_type.StartsWithIgnoringASCIICase("image/") ||
      mime_type.StartsWithIgnoringASCIICase("text/csv") ||
      mime_type.StartsWithIgnoringASCIICase("audio/") ||
      mime_type.StartsWithIgnoringASCIICase("video/")) {
    execution_context->AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel,
        "Refused to execute script from '" + response.Url().ElidedString() +
            "' because its MIME type ('" + mime_type +
            "') is not executable."));
    if (mime_type.StartsWithIgnoringASCIICase("image/")) {
      UseCounter::Count(execution_context,
                        WebFeature::kBlockedSniffingImageToScript);
    } else if (mime_type.StartsWithIgnoringASCIICase("audio/")) {
      UseCounter::Count(execution_context,
                        WebFeature::kBlockedSniffingAudioToScript);
    } else if (mime_type.StartsWithIgnoringASCIICase("video/")) {
      UseCounter::Count(execution_context,
                        WebFeature::kBlockedSniffingVideoToScript);
    } else if (mime_type.StartsWithIgnoringASCIICase("text/csv")) {
      UseCounter::Count(execution_context,
                        WebFeature::kBlockedSniffingCSVToScript);
    }
    return false;
  }

  // Beyond this point, we accept the given MIME type. Since we also accept
  // some legacy types, We want to log some of them using UseCounter to
  // support deprecation decisions.

  if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type))
    return true;

  if (mime_type.StartsWithIgnoringASCIICase("text/") &&
      MIMETypeRegistry::IsLegacySupportedJavaScriptLanguage(
          mime_type.Substring(5))) {
    return true;
  }

  bool same_origin =
      execution_context->GetSecurityOrigin()->CanRequest(response.Url());
  bool worker_global_scope = execution_context->IsWorkerGlobalScope();

  // These record usages for two MIME types (without subtypes), per same/cross
  // origin.
  if (mime_type.StartsWithIgnoringASCIICase("application/")) {
    UseCounter::Count(execution_context, kApplicationFeatures[same_origin]);
  } else if (mime_type.StartsWithIgnoringASCIICase("text/")) {
    UseCounter::Count(execution_context, kTextFeatures[same_origin]);
  }

  // These record usages for several MIME types/subtypes, per same/cross origin,
  // and per worker/non-worker context.
  if (mime_type.StartsWithIgnoringASCIICase("application/octet-stream")) {
    UseCounter::Count(
        execution_context,
        kApplicationOctetStreamFeatures[same_origin][worker_global_scope]);
  } else if (mime_type.StartsWithIgnoringASCIICase("application/xml")) {
    UseCounter::Count(
        execution_context,
        kApplicationXmlFeatures[same_origin][worker_global_scope]);
  } else if (mime_type.StartsWithIgnoringASCIICase("text/html")) {
    UseCounter::Count(execution_context,
                      kTextHtmlFeatures[same_origin][worker_global_scope]);
  } else if (mime_type.StartsWithIgnoringASCIICase("text/plain")) {
    UseCounter::Count(execution_context,
                      kTextPlainFeatures[same_origin][worker_global_scope]);
  } else if (mime_type.StartsWithIgnoringCase("text/xml")) {
    UseCounter::Count(execution_context,
                      kTextXmlFeatures[same_origin][worker_global_scope]);
  }

  return true;
}

}  // namespace blink
