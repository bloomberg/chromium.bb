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
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "wtf/text/AtomicString.h"

namespace blink {

ModuleScriptLoader::ModuleScriptLoader(Modulator* modulator,
                                       ModuleScriptLoaderRegistry* registry,
                                       ModuleScriptLoaderClient* client)
    : m_modulator(modulator), m_registry(registry), m_client(client) {
  DCHECK(modulator);
  DCHECK(registry);
  DCHECK(client);
}

ModuleScriptLoader::~ModuleScriptLoader() {}

#if DCHECK_IS_ON()
const char* ModuleScriptLoader::stateToString(ModuleScriptLoader::State state) {
  switch (state) {
    case State::Initial:
      return "Initial";
    case State::Fetching:
      return "Fetching";
    case State::Finished:
      return "Finished";
  }
  NOTREACHED();
  return "";
}
#endif

void ModuleScriptLoader::advanceState(ModuleScriptLoader::State newState) {
  switch (m_state) {
    case State::Initial:
      DCHECK_EQ(newState, State::Fetching);
      break;
    case State::Fetching:
      DCHECK_EQ(newState, State::Finished);
      break;
    case State::Finished:
      NOTREACHED();
      break;
  }

#if DCHECK_IS_ON()
  RESOURCE_LOADING_DVLOG(1)
      << "ModuleLoader[" << m_url.getString() << "]::advanceState("
      << stateToString(m_state) << " -> " << stateToString(newState) << ")";
#endif
  m_state = newState;

  if (m_state == State::Finished) {
    m_registry->releaseFinishedLoader(this);
    m_client->notifyNewSingleModuleFinished(m_moduleScript);
    setResource(nullptr);
  }
}

void ModuleScriptLoader::fetch(const ModuleScriptFetchRequest& moduleRequest,
                               ResourceFetcher* fetcher,
                               ModuleGraphLevel level) {
  // https://html.spec.whatwg.org/#fetch-a-single-module-script

  // Step 4. Set moduleMap[url] to "fetching".
  advanceState(State::Fetching);

  // Step 5. Let request be a new request whose url is url, ...
  ResourceRequest resourceRequest(moduleRequest.url());
#if DCHECK_IS_ON()
  m_url = moduleRequest.url();
#endif

  // TODO(kouhei): handle "destination is destination,"

  // ... type is "script", ...
  // -> FetchResourceType is specified by ScriptResource::fetch

  // parser metadata is parser state,
  ResourceLoaderOptions options;
  options.parserDisposition = moduleRequest.parserState();
  // referrer is referrer,
  if (!moduleRequest.referrer().isNull()) {
    resourceRequest.setHTTPReferrer(SecurityPolicy::generateReferrer(
        m_modulator->referrerPolicy(), moduleRequest.url(),
        moduleRequest.referrer()));
  }
  // and client is fetch client settings object. -> set by ResourceFetcher

  // As initiator for module script fetch is not specified in HTML spec,
  // we specity "" as initiator per:
  // https://fetch.spec.whatwg.org/#concept-request-initiator
  const AtomicString& initiatorName = emptyAtom;

  FetchRequest fetchRequest(resourceRequest, initiatorName, options);
  // ... cryptographic nonce metadata is cryptographic nonce, ...
  fetchRequest.setContentSecurityPolicyNonce(moduleRequest.nonce());
  // Note: The fetch request's "origin" isn't specified in
  // https://html.spec.whatwg.org/#fetch-a-single-module-script
  // Thus, the "origin" is "client" per
  // https://fetch.spec.whatwg.org/#concept-request-origin
  // ... mode is "cors", ...
  // ... credentials mode is credentials mode, ...
  // TODO(tyoshino): FetchCredentialsMode should be used to communicate CORS
  // mode.
  CrossOriginAttributeValue crossOrigin =
      moduleRequest.credentialsMode() ==
              WebURLRequest::FetchCredentialsModeInclude
          ? CrossOriginAttributeUseCredentials
          : CrossOriginAttributeAnonymous;
  fetchRequest.setCrossOriginAccessControl(m_modulator->securityOrigin(),
                                           crossOrigin);

  // Module scripts are always async.
  fetchRequest.setDefer(FetchRequest::LazyLoad);

  // Step 6. If the caller specified custom steps to perform the fetch,
  // perform them on request, setting the is top-level flag if the top-level
  // module fetch flag is set. Return from this algorithm, and when the custom
  // perform the fetch steps complete with response response, run the remaining
  // steps.
  // Otherwise, fetch request. Return from this algorithm, and run the remaining
  // steps as part of the fetch's process response for the response response.
  // TODO(ServiceWorker team): Perform the "custom steps" for module usage
  // inside service worker.
  m_nonce = moduleRequest.nonce();
  m_parserState = moduleRequest.parserState();

  ScriptResource* resource = ScriptResource::fetch(fetchRequest, fetcher);
  if (m_state == State::Finished) {
    // ScriptResource::fetch() has succeeded synchronously,
    // ::notifyFinished() already took care of the |resource|.
    return;
  }

  if (!resource) {
    // ScriptResource::fetch() has failed synchronously.
    advanceState(State::Finished);
    return;
  }

  // ScriptResource::fetch() is processed asynchronously.
  setResource(resource);
}

bool ModuleScriptLoader::wasModuleLoadSuccessful(Resource* resource) {
  // Implements conditions in Step 7 of
  // https://html.spec.whatwg.org/#fetch-a-single-module-script

  // - response's type is "error"
  if (resource->errorOccurred()) {
    return false;
  }

  const auto& response = resource->response();
  // - response's status is not an ok status
  if (response.isHTTP() && !FetchUtils::isOkStatus(response.httpStatusCode())) {
    return false;
  }

  // The result of extracting a MIME type from response's header list
  // (ignoring parameters) is not a JavaScript MIME type
  // Note: For historical reasons, fetching a classic script does not include
  // MIME type checking. In contrast, module scripts will fail to load if they
  // are not of a correct MIME type.
  // We use ResourceResponse::httpContentType() instead of mimeType(), as
  // mimeType() may be rewritten by mime sniffer.
  if (!MIMETypeRegistry::isSupportedJavaScriptMIMEType(
          response.httpContentType()))
    return false;

  return true;
}

// ScriptResourceClient callback handler
void ModuleScriptLoader::notifyFinished(Resource*) {
  // Note: "conditions" referred in Step 7 is implemented in
  // wasModuleLoadSuccessful().
  // Step 7. If any of the following conditions are met, set moduleMap[url] to
  // null, asynchronously complete this algorithm with null, and abort these
  // steps.
  if (!wasModuleLoadSuccessful(resource())) {
    advanceState(State::Finished);
    return;
  }

  // Step 8. Let source text be the result of UTF-8 decoding response's body.
  String sourceText = resource()->script();

  // Step 9. Let module script be the result of creating a module script given
  // source text, module map settings object, response's url, cryptographic
  // nonce, parser state, and credentials mode.
  m_moduleScript = createModuleScript(
      sourceText, resource()->response().url(), m_modulator, m_nonce,
      m_parserState, resource()->resourceRequest().fetchCredentialsMode());

  advanceState(State::Finished);
}

// https://html.spec.whatwg.org/#creating-a-module-script
ModuleScript* ModuleScriptLoader::createModuleScript(
    const String& sourceText,
    const KURL& url,
    Modulator* modulator,
    const String& nonce,
    ParserDisposition parserState,
    WebURLRequest::FetchCredentialsMode credentialsMode) {
  // Step 1. Let script be a new module script that this algorithm will
  // subsequently initialize.
  // Step 2. Set script's settings object to the environment settings object
  // provided.
  // Note: "script's settings object" will be "modulator".

  // Delegate to Modulator::compileModule to process Steps 3-6.
  ScriptModule result = modulator->compileModule(sourceText, url.getString());
  // Step 6: "...return null, and abort these steps."
  if (result.isNull())
    return nullptr;
  // Step 7. Set script's module record to result.
  // Step 8. Set script's base URL to the script base URL provided.
  // Step 9. Set script's cryptographic nonce to the cryptographic nonce
  // provided.
  // Step 10. Set script's parser state to the parser state.
  // Step 11. Set script's credentials mode to the credentials mode provided.
  // Step 12. Return script.
  return ModuleScript::create(result, url, nonce, parserState, credentialsMode);
}

DEFINE_TRACE(ModuleScriptLoader) {
  visitor->trace(m_modulator);
  visitor->trace(m_moduleScript);
  visitor->trace(m_registry);
  visitor->trace(m_client);
  ResourceOwner<ScriptResource>::trace(visitor);
}

}  // namespace blink
