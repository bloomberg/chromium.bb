// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleScriptFetcher.h"

#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/network/mime/MIMETypeRegistry.h"

namespace blink {

namespace {

bool WasModuleLoadSuccessful(Resource* resource,
                             ConsoleMessage** error_message) {
  // Implements conditions in Step 7 of
  // https://html.spec.whatwg.org/#fetch-a-single-module-script

  // - response's type is "error"
  if (!resource || resource->ErrorOccurred()) {
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
    if (error_message) {
      String message =
          "Failed to load module script: The server responded with a "
          "non-JavaScript MIME type of \"" +
          response.HttpContentType() +
          "\". Strict MIME type checking is enforced for module scripts per "
          "HTML spec.";
      *error_message = ConsoleMessage::CreateForRequest(
          kJSMessageSource, kErrorMessageLevel, message,
          response.Url().GetString(), resource->Identifier());
    }
    return false;
  }

  return true;
}

}  // namespace

ModuleScriptFetcher::ModuleScriptFetcher(const FetchParameters& fetch_params,
                                         ResourceFetcher* fetcher,
                                         Client* client)
    : fetch_params_(fetch_params),
      fetcher_(fetcher),
      client_(client) {}

void ModuleScriptFetcher::Fetch() {
  ScriptResource* resource = ScriptResource::Fetch(fetch_params_, fetcher_);
  if (was_fetched_) {
    // ScriptResource::Fetch() has succeeded synchronously,
    // ::NotifyFinished() already took care of the |resource|.
    return;
  }
  if (!resource) {
    // ScriptResource::Fetch() has failed synchronously.
    NotifyFinished(nullptr /* resource */);
    return;
  }

  // ScriptResource::Fetch() is processed asynchronously.
  SetResource(resource);
}

void ModuleScriptFetcher::NotifyFinished(Resource* resource) {
  ClearResource();

  ScriptResource* script_resource = ToScriptResource(resource);
  ConsoleMessage* error_message = nullptr;
  if (!WasModuleLoadSuccessful(script_resource, &error_message)) {
    Finalize(WTF::nullopt, error_message);
    return;
  }

  ModuleScriptCreationParams params(
      script_resource->GetResponse().Url(), script_resource->SourceText(),
      script_resource->GetResourceRequest().GetFetchCredentialsMode(),
      script_resource->CalculateAccessControlStatus());
  Finalize(params, nullptr /* error_message */);
}

void ModuleScriptFetcher::Finalize(
    const WTF::Optional<ModuleScriptCreationParams>& params,
    ConsoleMessage* error_message) {
  was_fetched_ = true;
  client_->NotifyFetchFinished(params, error_message);
}

DEFINE_TRACE(ModuleScriptFetcher) {
  visitor->Trace(fetcher_);
  visitor->Trace(client_);
  ResourceOwner<ScriptResource>::Trace(visitor);
}

}  // namespace blink
