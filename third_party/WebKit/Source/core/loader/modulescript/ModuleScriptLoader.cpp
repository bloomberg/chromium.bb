// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleScriptLoader.h"

#include "core/dom/Modulator.h"
#include "core/dom/ModuleScript.h"
#include "core/loader/modulescript/ModuleScriptLoaderClient.h"
#include "core/loader/modulescript/ModuleScriptLoaderRegistry.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

ModuleScriptLoader::ModuleScriptLoader(Modulator* modulator,
                                       ModuleScriptLoaderRegistry* registry,
                                       ModuleScriptLoaderClient* client)
    : modulator_(modulator), registry_(registry), client_(client) {
  DCHECK(modulator);
  DCHECK(registry);
  DCHECK(client);
}

ModuleScriptLoader::~ModuleScriptLoader() {}

#if DCHECK_IS_ON()
const char* ModuleScriptLoader::StateToString(ModuleScriptLoader::State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kFetching:
      return "Fetching";
    case State::kFinished:
      return "Finished";
  }
  NOTREACHED();
  return "";
}
#endif

void ModuleScriptLoader::AdvanceState(ModuleScriptLoader::State new_state) {
  switch (state_) {
    case State::kInitial:
      DCHECK_EQ(new_state, State::kFetching);
      break;
    case State::kFetching:
      DCHECK_EQ(new_state, State::kFinished);
      break;
    case State::kFinished:
      NOTREACHED();
      break;
  }

#if DCHECK_IS_ON()
  RESOURCE_LOADING_DVLOG(1)
      << "ModuleLoader[" << url_.GetString() << "]::advanceState("
      << StateToString(state_) << " -> " << StateToString(new_state) << ")";
#endif
  state_ = new_state;

  if (state_ == State::kFinished) {
    registry_->ReleaseFinishedLoader(this);
    client_->NotifyNewSingleModuleFinished(module_script_);
    SetResource(nullptr);
  }
}

void ModuleScriptLoader::Fetch(const ModuleScriptFetchRequest& module_request,
                               ResourceFetcher* fetcher,
                               ModuleGraphLevel level) {
  // https://html.spec.whatwg.org/#fetch-a-single-module-script

  // Step 4. Set moduleMap[url] to "fetching".
  AdvanceState(State::kFetching);

  // Step 5. Let request be a new request whose url is url, ...
  ResourceRequest resource_request(module_request.Url());
#if DCHECK_IS_ON()
  url_ = module_request.Url();
#endif

  // TODO(kouhei): handle "destination is destination,"

  // ... type is "script", ...
  // -> FetchResourceType is specified by ScriptResource::fetch

  // parser metadata is parser state,
  ResourceLoaderOptions options;
  options.parser_disposition = module_request.ParserState();
  // As initiator for module script fetch is not specified in HTML spec,
  // we specity "" as initiator per:
  // https://fetch.spec.whatwg.org/#concept-request-initiator
  options.initiator_info.name = g_empty_atom;

  if (level == ModuleGraphLevel::kDependentModuleFetch) {
    options.initiator_info.imported_module_referrer =
        module_request.GetReferrer();
    options.initiator_info.position = module_request.GetReferrerPosition();
  }

  // referrer is referrer,
  if (!module_request.GetReferrer().IsNull()) {
    resource_request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
        modulator_->GetReferrerPolicy(), module_request.Url(),
        module_request.GetReferrer()));
  }
  // and client is fetch client settings object. -> set by ResourceFetcher

  FetchParameters fetch_params(resource_request, options);
  // ... cryptographic nonce metadata is cryptographic nonce, ...
  fetch_params.SetContentSecurityPolicyNonce(module_request.Nonce());
  // Note: The fetch request's "origin" isn't specified in
  // https://html.spec.whatwg.org/#fetch-a-single-module-script
  // Thus, the "origin" is "client" per
  // https://fetch.spec.whatwg.org/#concept-request-origin
  // ... mode is "cors", ...
  // ... credentials mode is credentials mode, ...
  fetch_params.SetCrossOriginAccessControl(modulator_->GetSecurityOrigin(),
                                           module_request.CredentialsMode());

  // Module scripts are always async.
  fetch_params.SetDefer(FetchParameters::kLazyLoad);

  // Use UTF-8, according to Step 8:
  // "Let source text be the result of UTF-8 decoding response's body."
  fetch_params.SetDecoderOptions(
      TextResourceDecoderOptions::CreateAlwaysUseUTF8ForText());

  // Step 6. If the caller specified custom steps to perform the fetch,
  // perform them on request, setting the is top-level flag if the top-level
  // module fetch flag is set. Return from this algorithm, and when the custom
  // perform the fetch steps complete with response response, run the remaining
  // steps.
  // Otherwise, fetch request. Return from this algorithm, and run the remaining
  // steps as part of the fetch's process response for the response response.
  // TODO(ServiceWorker team): Perform the "custom steps" for module usage
  // inside service worker.
  nonce_ = module_request.Nonce();
  parser_state_ = module_request.ParserState();

  ScriptResource* resource = ScriptResource::Fetch(fetch_params, fetcher);
  if (state_ == State::kFinished) {
    // ScriptResource::fetch() has succeeded synchronously,
    // ::notifyFinished() already took care of the |resource|.
    return;
  }

  if (!resource) {
    // ScriptResource::fetch() has failed synchronously.
    AdvanceState(State::kFinished);
    return;
  }

  // ScriptResource::fetch() is processed asynchronously.
  SetResource(resource);
}

bool ModuleScriptLoader::WasModuleLoadSuccessful(Resource* resource) {
  // Implements conditions in Step 7 of
  // https://html.spec.whatwg.org/#fetch-a-single-module-script

  // - response's type is "error"
  if (resource->ErrorOccurred()) {
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
  // We use ResourceResponse::httpContentType() instead of mimeType(), as
  // mimeType() may be rewritten by mime sniffer.
  if (!MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
          response.HttpContentType()))
    return false;

  return true;
}

// ScriptResourceClient callback handler
void ModuleScriptLoader::NotifyFinished(Resource*) {
  // Note: "conditions" referred in Step 7 is implemented in
  // wasModuleLoadSuccessful().
  // Step 7. If any of the following conditions are met, set moduleMap[url] to
  // null, asynchronously complete this algorithm with null, and abort these
  // steps.
  if (!WasModuleLoadSuccessful(GetResource())) {
    AdvanceState(State::kFinished);
    return;
  }

  // Step 8. Let source text be the result of UTF-8 decoding response's body.
  String source_text = GetResource()->SourceText();

  AccessControlStatus access_control_status =
      GetResource()->CalculateAccessControlStatus(
          modulator_->GetSecurityOrigin());

  // Step 9. Let module script be the result of creating a module script given
  // source text, module map settings object, response's url, cryptographic
  // nonce, parser state, and credentials mode.
  module_script_ = ModuleScript::Create(
      source_text, modulator_, GetResource()->GetResponse().Url(), nonce_,
      parser_state_,
      GetResource()->GetResourceRequest().GetFetchCredentialsMode(),
      access_control_status);

  AdvanceState(State::kFinished);
}

DEFINE_TRACE(ModuleScriptLoader) {
  visitor->Trace(modulator_);
  visitor->Trace(module_script_);
  visitor->Trace(registry_);
  visitor->Trace(client_);
  ResourceOwner<ScriptResource>::Trace(visitor);
}

}  // namespace blink
