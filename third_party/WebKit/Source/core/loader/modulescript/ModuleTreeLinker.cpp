// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleTreeLinker.h"

#include "bindings/core/v8/ScriptModule.h"
#include "core/dom/ModuleScript.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleTreeLinkerRegistry.h"
#include "platform/WebTaskRunner.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/wtf/Vector.h"
#include "v8/include/v8.h"

namespace blink {

// Note: The current implementation is based on a mixture of the HTML specs
// of a little different versions, in order to incrementally update the code
// structure and the behavior.
//
// The followings are based on the spec BEFORE
// https://github.com/whatwg/html/pull/2991:
// - The cited spec statements of [IMSGF] and [FD].
// - The behavior (of the whole module script implementation in Blink).
//
// The followings are based on the spec AFTER that spec PR:
// - The cited spec statements of [FDaI] and [FFPE].
// - The code structure of Instantiate() and FindFirstParseError().
//   These methods are written based on the structure of the latest spec but
//   emunates the old behavior.
//
// TODO(hiroshige): The things based on the old spec should be updated shortly.

ModuleTreeLinker* ModuleTreeLinker::Fetch(
    const ModuleScriptFetchRequest& request,
    Modulator* modulator,
    ModuleTreeLinkerRegistry* registry,
    ModuleTreeClient* client) {
  ModuleTreeLinker* fetcher = new ModuleTreeLinker(modulator, registry, client);
  fetcher->FetchRoot(request);
  return fetcher;
}

ModuleTreeLinker* ModuleTreeLinker::FetchDescendantsForInlineScript(
    ModuleScript* module_script,
    Modulator* modulator,
    ModuleTreeLinkerRegistry* registry,
    ModuleTreeClient* client) {
  DCHECK(module_script);
  ModuleTreeLinker* fetcher = new ModuleTreeLinker(modulator, registry, client);
  fetcher->FetchRootInline(module_script);
  return fetcher;
}

ModuleTreeLinker::ModuleTreeLinker(Modulator* modulator,
                                   ModuleTreeLinkerRegistry* registry,
                                   ModuleTreeClient* client)
    : modulator_(modulator), registry_(registry), client_(client) {
  CHECK(modulator);
  CHECK(registry);
  CHECK(client);
}

void ModuleTreeLinker::Trace(blink::Visitor* visitor) {
  visitor->Trace(modulator_);
  visitor->Trace(registry_);
  visitor->Trace(client_);
  visitor->Trace(result_);
  SingleModuleClient::Trace(visitor);
}

void ModuleTreeLinker::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(result_);
}

#if DCHECK_IS_ON()
const char* ModuleTreeLinker::StateToString(ModuleTreeLinker::State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kFetchingSelf:
      return "FetchingSelf";
    case State::kFetchingDependencies:
      return "FetchingDependencies";
    case State::kInstantiating:
      return "Instantiating";
    case State::kFinished:
      return "Finished";
  }
  NOTREACHED();
  return "";
}
#endif

void ModuleTreeLinker::AdvanceState(State new_state) {
#if DCHECK_IS_ON()
  RESOURCE_LOADING_DVLOG(1)
      << *this << "::advanceState(" << StateToString(state_) << " -> "
      << StateToString(new_state) << ")";
#endif

  switch (state_) {
    case State::kInitial:
      CHECK_EQ(num_incomplete_fetches_, 0u);
      CHECK_EQ(new_state, State::kFetchingSelf);
      break;
    case State::kFetchingSelf:
      CHECK_EQ(num_incomplete_fetches_, 0u);
      CHECK(new_state == State::kFetchingDependencies ||
            new_state == State::kFinished);
      break;
    case State::kFetchingDependencies:
      CHECK(new_state == State::kInstantiating ||
            new_state == State::kFinished);
      break;
    case State::kInstantiating:
      CHECK_EQ(new_state, State::kFinished);
      break;
    case State::kFinished:
      NOTREACHED();
      break;
  }

  state_ = new_state;

  if (state_ == State::kFinished) {
#if DCHECK_IS_ON()
    if (result_) {
      RESOURCE_LOADING_DVLOG(1)
          << *this << " finished with final result " << *result_;
    } else {
      RESOURCE_LOADING_DVLOG(1) << *this << " finished with nullptr.";
    }
#endif

    registry_->ReleaseFinishedFetcher(this);

    // [IMSGF] Step 7. When the appropriate algorithm asynchronously completes
    // with final result, asynchronously complete this algorithm with final
    // result.
    client_->NotifyModuleTreeLoadFinished(result_);
  }
}

void ModuleTreeLinker::FetchRoot(const ModuleScriptFetchRequest& request) {
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-script-tree
#if DCHECK_IS_ON()
  url_ = request.Url();
  root_is_inline_ = false;
#endif

  AdvanceState(State::kFetchingSelf);

  // Step 1 is done in InitiateInternalModuleScriptGraphFetching().

  // Step 2. Perform the internal module script graph fetching procedure given
  // ... with the top-level module fetch flag set. ...
  InitiateInternalModuleScriptGraphFetching(
      request, ModuleGraphLevel::kTopLevelModuleFetch);
}

void ModuleTreeLinker::FetchRootInline(ModuleScript* module_script) {
  // Top-level entry point for [FDaI] for an inline module script.
  DCHECK(module_script);
#if DCHECK_IS_ON()
  url_ = module_script->BaseURL();
  root_is_inline_ = true;
#endif

  AdvanceState(State::kFetchingSelf);

  // Store the |module_script| here which will be used as result of the
  // algorithm when success. Also, this ensures that the |module_script| is
  // TraceWrappers()ed via ModuleTreeLinker.
  result_ = module_script;
  AdvanceState(State::kFetchingDependencies);

  modulator_->TaskRunner()->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&ModuleTreeLinker::FetchDescendants, WrapPersistent(this),
                WrapPersistent(module_script)));
}

void ModuleTreeLinker::InitiateInternalModuleScriptGraphFetching(
    const ModuleScriptFetchRequest& request,
    ModuleGraphLevel level) {
  DCHECK(!visited_set_.Contains(request.Url()));

  // This step originates from the callers of [IMSGF]:
  //
  // https://html.spec.whatwg.org/#fetch-a-module-script-tree
  // https://html.spec.whatwg.org/#fetch-a-module-worker-script-tree
  // Step 1. Let visited set be << url >>.
  //
  // [FD] Step 5.3.2. Append url to visited set.
  visited_set_.insert(request.Url());

  // [IMSGF] Step 1. Assert: visited set contains url.
  //
  // This is ensured by the insert() just above.

  ++num_incomplete_fetches_;

  // [IMSGF] Step 2. Fetch a single module script given ...
  modulator_->FetchSingle(request, level, this);

  // [IMSGF] Step 3-- are executed when NotifyModuleLoadFinished() is called.
}

void ModuleTreeLinker::NotifyModuleLoadFinished(ModuleScript* module_script) {
  // [IMSGF] Step 3. Return from this algorithm, and run the following steps
  // when fetching a single module script asynchronously completes with result:

  CHECK_GT(num_incomplete_fetches_, 0u);
  --num_incomplete_fetches_;

#if DCHECK_IS_ON()
  if (module_script) {
    RESOURCE_LOADING_DVLOG(1)
        << *this << "::NotifyModuleLoadFinished() with " << *module_script;
  } else {
    RESOURCE_LOADING_DVLOG(1)
        << *this << "::NotifyModuleLoadFinished() with nullptr.";
  }
#endif

  if (state_ == State::kFetchingSelf) {
    // Corresponds to top-level calls to
    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-and-instantiate-a-module-script
    // i.e. [IMSGF] with the top-level module fetch flag set (external), or
    // Step 22 of "prepare a script" (inline).
    // |module_script| is the top-level module, and will be instantiated
    // and returned later.
    result_ = module_script;
    AdvanceState(State::kFetchingDependencies);
  }

  if (state_ != State::kFetchingDependencies) {
    // We may reach here if one of the descendant failed to load, and the other
    // descendants fetches were in flight.
    return;
  }

  // Note: top-level module fetch flag is implemented so that Instantiate()
  // is called once after all descendants are fetched, which corresponds to
  // the single invocation of "fetch the descendants of and instantiate".

  // [IMSGF] Steps 4 and 5 are merged to FetchDescendants().

  // [IMSGF] Step 6. If the top-level module fetch flag is set, fetch the
  // descendants of and instantiate result given destination and visited set.
  // Otherwise, fetch the descendants of result given the same arguments.
  FetchDescendants(module_script);
}

void ModuleTreeLinker::FetchDescendants(ModuleScript* module_script) {
  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    result_ = nullptr;
    AdvanceState(State::kFinished);
    return;
  }

  // [FD] Step 1. If module script is errored or has instantiated,
  // asynchronously complete this algorithm with module script, and abort these
  // steps.
  //
  // [IMSGF] Step 4. If result is null, is errored, or has instantiated,
  // asynchronously complete this algorithm with result, and abort these steps.
  if (!module_script || module_script->IsErrored()) {
    found_error_ = true;
    // We don't early-exit here and wait until all module scripts to be
    // loaded, because we might be not sure which error to be reported.
    //
    // It is possible to determine whether the error to be reported can be
    // determined without waiting for loading module scripts, and thus to
    // early-exit here if possible. However, the complexity of such early-exit
    // implementation might be high, and optimizing error cases with the
    // implementation cost might be not worth doing.
    FinalizeFetchDescendantsForOneModuleScript();
    return;
  }
  if (module_script->HasInstantiated()) {
    FinalizeFetchDescendantsForOneModuleScript();
    return;
  }

  // [IMSGF] Step 5. Assert: result's record's [[Status]] is "uninstantiated".
  DCHECK_EQ(ScriptModuleState::kUninstantiated, module_script->RecordStatus());

  // [FD] Step 2. Let record be module script's record.
  ScriptModule record = module_script->Record();
  DCHECK(!record.IsNull());

  // [FD] Step 3. If record.[[RequestedModules]] is empty, asynchronously
  // complete this algorithm with module script.
  //
  // Note: We defer this bail-out until the end of the procedure. The rest of
  // the procedure will be no-op anyway if record.[[RequestedModules]] is empty.

  // [FD] Step 4. Let urls be a new empty list.
  Vector<KURL> urls;
  Vector<TextPosition> positions;

  // [FD] Step 5. For each string requested of record.[[RequestedModules]],
  Vector<Modulator::ModuleRequest> module_requests =
      modulator_->ModuleRequestsFromScriptModule(record);
  for (const auto& module_request : module_requests) {
    // [FD] Step 5.1. Let url be the result of resolving a module specifier
    // given module script and requested.
    KURL url = Modulator::ResolveModuleSpecifier(module_request.specifier,
                                                 module_script->BaseURL());

    // [FD] Step 5.2. Assert: url is never failure, because resolving a module
    // specifier must have been previously successful with these same two
    // arguments.
    CHECK(url.IsValid()) << "Modulator::resolveModuleSpecifier() impl must "
                            "return either a valid url or null.";

    // [FD] Step 5.3. If visited set does not contain url, then:
    if (!visited_set_.Contains(url)) {
      // [FD] Step 5.3.1. Append url to urls.
      urls.push_back(url);

      // [FD] Step 5.3.2. Append url to visited set.
      //
      // This step is deferred to InitiateInternalModuleScriptGraphFetching()
      // below.

      positions.push_back(module_request.position);
    }
  }

  if (urls.IsEmpty()) {
    // [FD] Step 3. If record.[[RequestedModules]] is empty, asynchronously
    // complete this algorithm with module script.
    //
    // Also, if record.[[RequestedModules]] is not empty but |urls| is
    // empty here, we complete this algorithm.
    FinalizeFetchDescendantsForOneModuleScript();
    return;
  }

  // [FD] Step 6. Let options be the descendant script fetch options for module
  // script's fetch options.
  // https://html.spec.whatwg.org/multipage/webappapis.html#descendant-script-fetch-options
  // the descendant script fetch options are a new script fetch options whose
  // items all have the same values, except for the integrity metadata, which is
  // instead the empty string.
  ScriptFetchOptions options(module_script->FetchOptions().Nonce(),
                             IntegrityMetadataSet(), String(),
                             module_script->FetchOptions().ParserState(),
                             module_script->FetchOptions().CredentialsMode());

  // [FD] Step 7. For each url in urls, ...
  //
  // [FD] Step 7. These invocations of the internal module script graph fetching
  // procedure should be performed in parallel to each other.
  for (size_t i = 0; i < urls.size(); ++i) {
    // [FD] Step 7. ... perform the internal module script graph fetching
    // procedure given ... with the top-level module fetch flag unset. ...
    ModuleScriptFetchRequest request(
        urls[i], options, module_script->BaseURL().GetString(),
        modulator_->GetReferrerPolicy(), positions[i]);
    InitiateInternalModuleScriptGraphFetching(
        request, ModuleGraphLevel::kDependentModuleFetch);
  }

  // Asynchronously continue processing after NotifyModuleLoadFinished() is
  // called num_incomplete_fetches_ times.
  CHECK_GT(num_incomplete_fetches_, 0u);
}

void ModuleTreeLinker::FinalizeFetchDescendantsForOneModuleScript() {
  // [FD] of a single module script is completed here:
  //
  // [FD] Step 6. Wait for all invocations of the internal module script graph
  // fetching procedure to asynchronously complete, ...

  // And, if |num_incomplete_fetches_| is 0, all the invocations of [FD]
  // (called from [FDaI] Step 2) of the root module script is completed here
  // and thus we proceed to [FDaI] Step 4 implemented by Instantiate().
  if (num_incomplete_fetches_ == 0)
    Instantiate();
}

void ModuleTreeLinker::Instantiate() {
  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    result_ = nullptr;
    AdvanceState(State::kFinished);
    return;
  }

  // [FDaI] Step 4. If result is null, then asynchronously complete this
  // algorithm with result.
  if (!result_) {
    AdvanceState(State::kFinished);
    return;
  }

  // [FDaI] Step 5. Let parse error be the result of finding the first parse
  // error given result.
  //
  // [Optimization] If |found_error_| is false (i.e. no errors were found during
  // fetching), we are sure that |parse error| is null and thus skip
  // FindFirstParseError() call.
  if (found_error_) {
    // [FFPE] Step 2. If discoveredSet was not given, let it be an empty set.
    HeapHashSet<Member<ModuleScript>> discovered_set;
    bool found_first_error = FindFirstParseError(result_, &discovered_set);
    DCHECK(found_first_error);
    DCHECK(!result_ || result_->IsErrored());
  }

  // [FDaI] Step 6. If parse error is null, then:
  //
  // [FDaI] Step 7. Otherwise, set result's error to rethrow to parse error.
  //
  // [old spec] TODO(hiroshige): Update this.
  if (!result_ || result_->IsErrored()) {
    AdvanceState(State::kFinished);
    return;
  }

  // In the case of parse error is not null:

  DCHECK(result_);
  DCHECK(!found_error_);
  AdvanceState(State::kInstantiating);

  // [FDaI] Step 6.1. Let record be result's record.
  ScriptModule record = result_->Record();

  // [FDaI] Step 6.2. Perform record.Instantiate(). If this throws an
  // exception, set result's error to rethrow to that exception.
  modulator_->InstantiateModule(record);

  // [FDaI] Step 8. Asynchronously complete this algorithm with result.
  AdvanceState(State::kFinished);
}

// [FFPE] https://html.spec.whatwg.org/#finding-the-first-parse-error
//
// TODO(hiroshige): The code structure below aligns the spec after
// https://github.com/whatwg/html/pull/2991, but the behavior aligns the
// spec before that PR (that originates from Step 6.1. of [FD]), and thus
// contains [old spec] statements.
// Update the behavior according to the PR.
//
// This returns true if an error is found, and updates |result_| or
// |result_|'s error accordingly.
//
// TODO(hiroshige): This is also because the behavior is based on the old spec.
// Make FindFirstParseError to return ScriptValue once the behavior is updated.
bool ModuleTreeLinker::FindFirstParseError(
    ModuleScript* module_script,
    HeapHashSet<Member<ModuleScript>>* discovered_set) {
  // [FFPE] Step 1. Let moduleMap be moduleScript's settings object's module
  // map.
  //
  // This is accessed via |modulator_|.

  // [FFPE] Step 2 is done before calling this in Instantiate().

  // [old spec] [FD] Step 6.1. If result is null, asynchronously complete this
  // algorithm with null, aborting these steps.
  if (!module_script) {
    result_ = nullptr;
    return true;
  }

  // [FFPE] Step 3. Append moduleScript to discoveredSet.
  discovered_set->insert(module_script);

  // [FFPE] Step 4. If moduleScript's record is null, then return moduleScript's
  // parse error.
  //
  // [old spec] [FD] Step 6.2. If result is errored, then set the
  // pre-instantiation error for module script to result's error. Asynchronously
  // complete this algorithm with module script, aborting these steps.
  if (module_script->IsErrored()) {
    result_->SetErrorAndClearRecord(modulator_->GetError(module_script));
    return true;
  }

  // [FFPE] Step 5. Let childSpecifiers be the value of moduleScript's record's
  // [[RequestedModules]] internal slot.
  ScriptModule record = module_script->Record();
  DCHECK(!record.IsNull());
  Vector<Modulator::ModuleRequest> child_specifiers =
      modulator_->ModuleRequestsFromScriptModule(record);

  for (const auto& module_request : child_specifiers) {
    // [FFPE] Step 6. Let childURLs be the list obtained by calling resolve a
    // module specifier once for each item of childSpecifiers, given
    // moduleScript and that item. ...
    KURL child_url = Modulator::ResolveModuleSpecifier(
        module_request.specifier, module_script->BaseURL());

    // [FFPE] Step 6. ... (None of these will ever fail, as otherwise
    // moduleScript would have been marked as itself having a parse error.)
    CHECK(child_url.IsValid())
        << "Modulator::ResolveModuleSpecifier() impl must "
           "return either a valid url or null.";

    // [FFPE] Step 7. Let childModules be the list obtained by getting each
    // value in moduleMap whose key is given by an item of childURLs.
    //
    // [FFPE] Step 8. For each childModule of childModules:
    ModuleScript* child_module = modulator_->GetFetchedModuleScript(child_url);

    // [FFPE] Step 8.2. If discoveredSet already contains childModule, continue.
    //
    // TODO(hiroshige): if |child_module| is null, we skip Contains() call
    // because HashSet forbids nullptr. Anyway |child_module| can be nullptr
    // because this is based on the [old spec], so remove this hack once we
    // update the behavior.
    if (child_module && discovered_set->Contains(child_module))
      continue;

    // [FFPE] Step 8.3. Let childParseError be the result of finding the first
    // parse error given childModule and discoveredSet.
    //
    // [FFPE] Step 8.4. If childParseError is not null, return childParseError.
    if (FindFirstParseError(child_module, discovered_set))
      return true;
  }

  // [FFPE] Step 9. Return null.
  return false;
}

#if DCHECK_IS_ON()
std::ostream& operator<<(std::ostream& stream, const ModuleTreeLinker& linker) {
  stream << "ModuleTreeLinker[" << &linker
         << ", url=" << linker.url_.GetString()
         << ", inline=" << linker.root_is_inline_ << "]";
  return stream;
}
#endif

}  // namespace blink
