// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/DocumentModuleScriptFetcher.h"

#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/SubresourceIntegrityHelper.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/network/mime/MIMETypeRegistry.h"

namespace blink {

namespace {

bool WasModuleLoadSuccessful(
    Resource* resource,
    HeapVector<Member<ConsoleMessage>>* error_messages) {
  // Implements conditions in Step 7 of
  // https://html.spec.whatwg.org/#fetch-a-single-module-script

  DCHECK(error_messages);

  if (resource) {
    SubresourceIntegrityHelper::GetConsoleMessages(
        resource->IntegrityReportInfo(), error_messages);
  }

  // - response's type is "error"
  if (!resource || resource->ErrorOccurred() ||
      resource->IntegrityDisposition() !=
          ResourceIntegrityDisposition::kPassed) {
    return false;
  }

  const auto& response = resource->GetResponse();
  // - response's status is not an ok status
  if (response.IsHTTP() && !FetchUtils::IsOkStatus(response.HttpStatusCode())) {
    return false;
  }

  // The result of extracting a MIME type from response's header list
  // (ignoring parameters) is not a JavaScript MIME type
  // Note: For historical reasons, fetching a classic script does not include
  // MIME type checking. In contrast, module scripts will fail to load if they
  // are not of a correct MIME type.
  // We use ResourceResponse::HttpContentType() instead of MimeType(), as
  // MimeType() may be rewritten by mime sniffer.
  if (!MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
          response.HttpContentType())) {
    String message =
        "Failed to load module script: The server responded with a "
        "non-JavaScript MIME type of \"" +
        response.HttpContentType() +
        "\". Strict MIME type checking is enforced for module scripts per "
        "HTML spec.";
    error_messages->push_back(ConsoleMessage::CreateForRequest(
        kJSMessageSource, kErrorMessageLevel, message,
        response.Url().GetString(), nullptr, resource->Identifier()));
    return false;
  }

  return true;
}

}  // namespace

DocumentModuleScriptFetcher::DocumentModuleScriptFetcher(
    ResourceFetcher* fetcher)
    : fetcher_(fetcher) {
  DCHECK(fetcher_);
}

void DocumentModuleScriptFetcher::Fetch(FetchParameters& fetch_params,
                                        ModuleScriptFetcher::Client* client) {
  SetClient(client);
  ScriptResource::Fetch(fetch_params, fetcher_, this);
}

void DocumentModuleScriptFetcher::NotifyFinished(Resource* resource) {
  ClearResource();

  ScriptResource* script_resource = ToScriptResource(resource);

  HeapVector<Member<ConsoleMessage>> error_messages;
  if (!WasModuleLoadSuccessful(script_resource, &error_messages)) {
    Finalize(WTF::nullopt, error_messages);
    return;
  }

  ModuleScriptCreationParams params(
      script_resource->GetResponse().Url(), script_resource->SourceText(),
      script_resource->GetResourceRequest().GetFetchCredentialsMode(),
      script_resource->CalculateAccessControlStatus(
          fetcher_->Context().GetSecurityOrigin()));
  Finalize(params, error_messages);
}

void DocumentModuleScriptFetcher::Finalize(
    const WTF::Optional<ModuleScriptCreationParams>& params,
    const HeapVector<Member<ConsoleMessage>>& error_messages) {
  NotifyFetchFinished(params, error_messages);
}

void DocumentModuleScriptFetcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetcher_);
  ResourceClient::Trace(visitor);
  ModuleScriptFetcher::Trace(visitor);
}

}  // namespace blink
