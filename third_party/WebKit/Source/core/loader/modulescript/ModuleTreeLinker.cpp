// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleTreeLinker.h"

#include "bindings/core/v8/ScriptModule.h"
#include "core/dom/AncestorList.h"
#include "core/dom/ModuleScript.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleTreeLinkerRegistry.h"
#include "core/loader/modulescript/ModuleTreeReachedUrlSet.h"
#include "platform/WebTaskRunner.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/wtf/Vector.h"
#include "v8/include/v8.h"

namespace blink {

ModuleTreeLinker* ModuleTreeLinker::Fetch(
    const ModuleScriptFetchRequest& request,
    const AncestorList& ancestor_list,
    ModuleGraphLevel level,
    Modulator* modulator,
    ModuleTreeReachedUrlSet* reached_url_set,
    ModuleTreeLinkerRegistry* registry,
    ModuleTreeClient* client) {
  AncestorList ancestor_list_with_url = ancestor_list;
  ancestor_list_with_url.insert(request.Url());

  ModuleTreeLinker* fetcher =
      new ModuleTreeLinker(ancestor_list_with_url, level, modulator,
                           reached_url_set, registry, client);
  fetcher->FetchSelf(request);
  return fetcher;
}

ModuleTreeLinker* ModuleTreeLinker::FetchDescendantsForInlineScript(
    ModuleScript* module_script,
    Modulator* modulator,
    ModuleTreeLinkerRegistry* registry,
    ModuleTreeClient* client) {
  AncestorList empty_ancestor_list;

  // Substep 4 in "module" case in Step 22 of "prepare a script":"
  // https://html.spec.whatwg.org/#prepare-a-script
  DCHECK(module_script);

  // 4. "Fetch the descendants of script (using an empty ancestor list)."
  ModuleTreeLinker* fetcher = new ModuleTreeLinker(
      empty_ancestor_list, ModuleGraphLevel::kTopLevelModuleFetch, modulator,
      nullptr, registry, client);
  fetcher->module_script_ = module_script;
  fetcher->AdvanceState(State::kFetchingSelf);

  // "When this asynchronously completes, set the script's script to
  //  the result. At that time, the script is ready."
  //
  // Currently we execute "internal module script graph
  // fetching procedure" Step 5- in addition to "fetch the descendants",
  // which is not specced yet. https://github.com/whatwg/html/issues/2544
  // TODO(hiroshige): Fix the implementation and/or comments once the spec
  // is updated.
  modulator->TaskRunner()->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&ModuleTreeLinker::FetchDescendants, WrapPersistent(fetcher)));
  return fetcher;
}

ModuleTreeLinker::ModuleTreeLinker(const AncestorList& ancestor_list_with_url,
                                   ModuleGraphLevel level,
                                   Modulator* modulator,
                                   ModuleTreeReachedUrlSet* reached_url_set,
                                   ModuleTreeLinkerRegistry* registry,
                                   ModuleTreeClient* client)
    : modulator_(modulator),
      reached_url_set_(
          level == ModuleGraphLevel::kTopLevelModuleFetch
              ? ModuleTreeReachedUrlSet::CreateFromTopLevelAncestorList(
                    ancestor_list_with_url)
              : reached_url_set),
      registry_(registry),
      client_(client),
      ancestor_list_with_url_(ancestor_list_with_url),
      level_(level),
      module_script_(this, nullptr) {
  CHECK(modulator);
  CHECK(reached_url_set_);
  CHECK(registry);
  CHECK(client);
}

DEFINE_TRACE(ModuleTreeLinker) {
  visitor->Trace(modulator_);
  visitor->Trace(reached_url_set_);
  visitor->Trace(registry_);
  visitor->Trace(client_);
  visitor->Trace(module_script_);
  visitor->Trace(dependency_clients_);
  SingleModuleClient::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(ModuleTreeLinker) {
  visitor->TraceWrappers(module_script_);
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
      << "ModuleTreeLinker[" << this << "]::advanceState("
      << StateToString(state_) << " -> " << StateToString(new_state) << ")";
#endif

  switch (state_) {
    case State::kInitial:
      CHECK_EQ(num_incomplete_descendants_, 0u);
      CHECK_EQ(new_state, State::kFetchingSelf);
      break;
    case State::kFetchingSelf:
      CHECK_EQ(num_incomplete_descendants_, 0u);
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
    if (module_script_) {
      RESOURCE_LOADING_DVLOG(1)
          << "ModuleTreeLinker[" << this << "] finished with final result "
          << *module_script_;
    } else {
      RESOURCE_LOADING_DVLOG(1)
          << "ModuleTreeLinker[" << this << "] finished with nullptr.";
    }

    registry_->ReleaseFinishedFetcher(this);

    // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure
    // Step 6. When the appropriate algorithm asynchronously completes with
    // final result, asynchronously complete this algorithm with final result.
    client_->NotifyModuleTreeLoadFinished(module_script_);
  }
}

void ModuleTreeLinker::FetchSelf(const ModuleScriptFetchRequest& request) {
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure

  // Step 1. Fetch a single module script given url, fetch client settings
  // object, destination, cryptographic nonce, parser state, credentials mode,
  // module map settings object, referrer, and the top-level module fetch flag.
  // If the caller of this algorithm specified custom perform the fetch steps,
  // pass those along while fetching a single module script.
  AdvanceState(State::kFetchingSelf);
  modulator_->FetchSingle(request, level_, this);

  // Step 2. Return from this algorithm, and run the following steps when
  // fetching a single module script asynchronously completes with result.
  // Note: Modulator::FetchSingle asynchronously notifies result to
  // ModuleTreeLinker::NotifyModuleLoadFinished().
}

void ModuleTreeLinker::NotifyModuleLoadFinished(ModuleScript* result) {
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure

  // Step 3. "If result is null, is errored, or has instantiated, asynchronously
  // complete this algorithm with result, and abort these steps.
  if (!result || result->IsErrored() || result->HasInstantiated()) {
    // "asynchronously complete this algorithm with result, and abort these
    // steps."
    module_script_ = result;
    AdvanceState(State::kFinished);
    return;
  }

  // Step 4. Assert: result's state is "uninstantiated".
  DCHECK_EQ(ScriptModuleState::kUninstantiated, result->RecordStatus());

  // Step 5. If the top-level module fetch flag is set, fetch the descendants of
  // and instantiate result given destination and an ancestor list obtained by
  // appending url to ancestor list. Otherwise, fetch the descendants of result
  // given the same arguments.
  // Note: top-level module fetch flag is checked at Instantiate(), where
  //       "fetch the descendants of and instantiate" procedure  and
  //       "fetch the descendants" procedure actually diverge.
  module_script_ = result;
  FetchDescendants();
}

class ModuleTreeLinker::DependencyModuleClient : public ModuleTreeClient {
 public:
  static DependencyModuleClient* Create(ModuleTreeLinker* module_tree_linker) {
    return new DependencyModuleClient(module_tree_linker);
  }
  virtual ~DependencyModuleClient() = default;

  DEFINE_INLINE_TRACE() {
    visitor->Trace(module_tree_linker_);
    visitor->Trace(result_);
    ModuleTreeClient::Trace(visitor);
  }

  ModuleScript* Result() { return result_.Get(); }

 private:
  explicit DependencyModuleClient(ModuleTreeLinker* module_tree_linker)
      : module_tree_linker_(module_tree_linker) {
    CHECK(module_tree_linker);
  }

  // Implements ModuleTreeClient
  void NotifyModuleTreeLoadFinished(ModuleScript*) override;

  Member<ModuleTreeLinker> module_tree_linker_;
  Member<ModuleScript> result_;
};

void ModuleTreeLinker::FetchDescendants() {
  CHECK(module_script_);
  AdvanceState(State::kFetchingDependencies);

  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    module_script_ = nullptr;
    AdvanceState(State::kFinished);
    return;
  }

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-a-module-script
  // Step 1. If ancestor list was not given, let it be the empty list.
  // Note: The "ancestor list" in spec corresponds to |ancestor_list_with_url_|.

  // Step 2. If module script is errored or has instantiated,
  // asynchronously complete this algorithm with module script, and abort these
  // steps.
  if (module_script_->IsErrored() || module_script_->HasInstantiated()) {
    AdvanceState(State::kFinished);
    return;
  }

  // Step 3. Let record be module script's module record.
  ScriptModule record = module_script_->Record();
  DCHECK(!record.IsNull());

  // Step 4. If record.[[RequestedModules]] is empty, asynchronously complete
  // this algorithm with module script.
  // Note: We defer this bail-out until the end of the procedure. The rest of
  //       the procedure will be no-op anyway if record.[[RequestedModules]]
  //       is empty.

  // Step 5. Let urls be a new empty list.
  Vector<KURL> urls;
  Vector<TextPosition> positions;

  // Step 6. For each string requested of record.[[RequestedModules]],
  Vector<Modulator::ModuleRequest> module_requests =
      modulator_->ModuleRequestsFromScriptModule(record);
  for (const auto& module_request : module_requests) {
    // Step 6.1. Let url be the result of resolving a module specifier given
    // module script and requested.
    KURL url = Modulator::ResolveModuleSpecifier(module_request.specifier,
                                                 module_script_->BaseURL());

    // Step 6.2. Assert: url is never failure, because resolving a module
    // specifier must have been previously successful with these same two
    // arguments.
    CHECK(url.IsValid()) << "Modulator::resolveModuleSpecifier() impl must "
                            "return either a valid url or null.";

    // Step 6.3. if ancestor list does not contain url, append url to urls.
    if (ancestor_list_with_url_.Contains(url))
      continue;

    // [unspec] If we already have started a sub-graph fetch for the |url| for
    // this top-level module graph starting from |top_level_linker_|, we can
    // safely rely on other module graph node ModuleTreeLinker to handle it.
    if (reached_url_set_->IsAlreadyBeingFetched(url)) {
      // We can't skip any sub-graph fetches directly made from the top level
      // module, as we may end up proceeding to ModuleDeclarationInstantiation()
      // with part of the graph still fetching.
      CHECK_NE(level_, ModuleGraphLevel::kTopLevelModuleFetch);

      continue;
    }

    urls.push_back(url);
    positions.push_back(module_request.position);
  }

  // Step 7. For each url in urls, perform the internal module script graph
  // fetching procedure given url, module script's credentials mode, module
  // script's cryptographic nonce, module script's parser state, destination,
  // module script's settings object, module script's settings object, ancestor
  // list, module script's base URL, and with the top-level module fetch flag
  // unset. If the caller of this algorithm specified custom perform the fetch
  // steps, pass those along while performing the internal module script graph
  // fetching procedure.

  if (urls.IsEmpty()) {
    // Step 4. If record.[[RequestedModules]] is empty, asynchronously
    // complete this algorithm with module script. [spec text]

    // Also, if record.[[RequestedModules]] is not empty but |urls| is
    // empty here, we complete this algorithm.
    Instantiate();
    return;
  }

  // Step 7, when "urls" is non-empty.
  // These invocations of the internal module script graph fetching procedure
  // should be performed in parallel to each other. [spec text]
  CHECK_EQ(num_incomplete_descendants_, 0u);
  num_incomplete_descendants_ = urls.size();
  for (size_t i = 0; i < urls.size(); ++i) {
    DependencyModuleClient* dependency_client =
        DependencyModuleClient::Create(this);
    reached_url_set_->ObserveModuleTreeLink(urls[i]);
    dependency_clients_.insert(dependency_client);

    ModuleScriptFetchRequest request(
        urls[i], module_script_->Nonce(), module_script_->ParserState(),
        module_script_->CredentialsMode(),
        module_script_->BaseURL().GetString(), positions[i]);
    modulator_->FetchTreeInternal(request, ancestor_list_with_url_,
                                  ModuleGraphLevel::kDependentModuleFetch,
                                  reached_url_set_.Get(), dependency_client);
  }

  // Asynchronously continue processing after NotifyOneDescendantFinished() is
  // called num_incomplete_descendants_ times.
  CHECK_GT(num_incomplete_descendants_, 0u);
}

void ModuleTreeLinker::DependencyModuleClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  result_ = module_script;
  if (module_script) {
    RESOURCE_LOADING_DVLOG(1)
        << "ModuleTreeLinker[" << module_tree_linker_.Get()
        << "]::DependencyModuleClient::NotifyModuleTreeLoadFinished() with "
        << *module_script;
  } else {
    RESOURCE_LOADING_DVLOG(1)
        << "ModuleTreeLinker[" << module_tree_linker_.Get()
        << "]::DependencyModuleClient::NotifyModuleTreeLoadFinished() with "
        << "nullptr.";
  }
  module_tree_linker_->NotifyOneDescendantFinished();
}

void ModuleTreeLinker::NotifyOneDescendantFinished() {
  if (state_ != State::kFetchingDependencies) {
    // We may reach here if one of the descendant failed to load, and the other
    // descendants fetches were in flight.
    return;
  }
  DCHECK_EQ(state_, State::kFetchingDependencies);
  DCHECK(module_script_);

  CHECK_GT(num_incomplete_descendants_, 0u);
  --num_incomplete_descendants_;

  RESOURCE_LOADING_DVLOG(1)
      << "ModuleTreeLinker[" << this << "]::NotifyOneDescendantFinished. "
      << num_incomplete_descendants_ << " remaining descendants.";

  // Step 7 of
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-a-module-script
  // "Wait for all invocations of the internal module script graph fetching
  // procedure to asynchronously complete, and ..." [spec text]
  if (num_incomplete_descendants_)
    return;

  // "let results be a list of the results, corresponding to the same order they
  // appeared in urls. Then, for each result of results:" [spec text]
  for (const auto& client : dependency_clients_) {
    ModuleScript* result = client->Result();

    // Step 7.1: "If result is null, ..." [spec text]
    if (!result) {
      // "asynchronously complete this algorithm with null, aborting these
      // steps." [spec text]
      module_script_ = nullptr;
      AdvanceState(State::kFinished);
      return;
    }

    // Step 7.2: "If result is errored, ..." [spec text]
    if (result->IsErrored()) {
      // "then set the pre-instantiation error for module script to result's
      // error ..." [spec text]
      ScriptValue error = modulator_->GetError(result);
      module_script_->SetErrorAndClearRecord(error);

      // "Asynchronously complete this algorithm with module script, aborting
      // these steps." [spec text]
      AdvanceState(State::kFinished);
      return;
    }
  }

  Instantiate();
}

void ModuleTreeLinker::Instantiate() {
  CHECK(module_script_);
  AdvanceState(State::kInstantiating);

  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure
  // Step 5. "If the top-level module fetch flag is set, fetch the descendants
  // of and instantiate result given destination and an ancestor list obtained
  // by appending url to ancestor list. Otherwise, fetch the descendants of
  // result given the same arguments." [spec text]
  if (level_ != ModuleGraphLevel::kTopLevelModuleFetch) {
    // We don't proceed to instantiate steps if this is descendants module graph
    // fetch.
    DCHECK_EQ(level_, ModuleGraphLevel::kDependentModuleFetch);
    AdvanceState(State::kFinished);
    return;
  }

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-and-instantiate-a-module-script
  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    module_script_ = nullptr;
    AdvanceState(State::kFinished);
    return;
  }

  // Step 1-2 are "fetching the descendants of a module script", which we just
  // executed.

  // Step 3. "If result is null or is errored, ..." [spec text]
  if (!module_script_ || module_script_->IsErrored()) {
    // "then asynchronously complete this algorithm with result." [spec text]
    module_script_ = nullptr;
    AdvanceState(State::kFinished);
    return;
  }

  // Step 4. "Let record be result's module record." [spec text]
  ScriptModule record = module_script_->Record();

  // Step 5. "Perform record.ModuleDeclarationInstantiation()." [spec text]

  // "If this throws an exception, ignore it for now; it is stored as result's
  // error, and will be reported when we run result." [spec text]
  modulator_->InstantiateModule(record);

  // Step 6. Asynchronously complete this algorithm with descendants result.
  AdvanceState(State::kFinished);
}

}  // namespace blink
