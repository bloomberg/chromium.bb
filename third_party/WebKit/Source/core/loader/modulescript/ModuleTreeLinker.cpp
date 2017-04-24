// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleTreeLinker.h"

#include "bindings/core/v8/ScriptModule.h"
#include "core/dom/AncestorList.h"
#include "core/dom/ModuleScript.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleTreeLinkerRegistry.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/wtf/Vector.h"

namespace blink {

ModuleTreeLinker* ModuleTreeLinker::Fetch(
    const ModuleScriptFetchRequest& request,
    const AncestorList& ancestor_list,
    ModuleGraphLevel level,
    Modulator* modulator,
    ModuleTreeLinkerRegistry* registry,
    ModuleTreeClient* client) {
  AncestorList ancestor_list_with_url = ancestor_list;
  ancestor_list_with_url.insert(request.Url());

  ModuleTreeLinker* fetcher =
      new ModuleTreeLinker(ancestor_list_with_url, modulator, registry, client);
  fetcher->FetchSelf(request, level);
  return fetcher;
}

ModuleTreeLinker::ModuleTreeLinker(const AncestorList& ancestor_list_with_url,
                                   Modulator* modulator,
                                   ModuleTreeLinkerRegistry* registry,
                                   ModuleTreeClient* client)
    : modulator_(modulator),
      registry_(registry),
      client_(client),
      ancestor_list_with_url_(ancestor_list_with_url) {
  CHECK(modulator);
  CHECK(registry);
  CHECK(client);
}

DEFINE_TRACE(ModuleTreeLinker) {
  visitor->Trace(modulator_);
  visitor->Trace(registry_);
  visitor->Trace(client_);
  visitor->Trace(module_script_);
  visitor->Trace(descendants_module_script_);
  visitor->Trace(dependency_clients_);
  SingleModuleClient::Trace(visitor);
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
            (!descendants_module_script_ && new_state == State::kFinished));
      break;
    case State::kFetchingDependencies:
      CHECK_EQ(new_state, State::kInstantiating);
      DCHECK(!num_incomplete_descendants_ || !descendants_module_script_)
          << num_incomplete_descendants_
          << " outstanding descendant loads found, but the descendant module "
             "script load procedure unexpectedly finished with "
          << (descendants_module_script_ ? "success." : "failure.");
      break;
    case State::kInstantiating:
      CHECK(num_incomplete_descendants_ == 0u || !descendants_module_script_);
      CHECK_EQ(new_state, State::kFinished);
      break;
    case State::kFinished:
      NOTREACHED();
      break;
  }

  state_ = new_state;

  if (state_ == State::kFinished) {
    registry_->ReleaseFinishedFetcher(this);

    // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure
    // Step 8. Asynchronously complete this algorithm with descendants result.
    client_->NotifyModuleTreeLoadFinished(descendants_module_script_);
  }
}

void ModuleTreeLinker::FetchSelf(const ModuleScriptFetchRequest& request,
                                 ModuleGraphLevel level) {
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure

  // Step 1. Fetch a single module script given url, fetch client settings
  // object, destination, cryptographic nonce, parser state, credentials mode,
  // module map settings object, referrer, and the top-level module fetch flag.
  // If the caller of this algorithm specified custom perform the fetch steps,
  // pass those along while fetching a single module script.
  AdvanceState(State::kFetchingSelf);
  modulator_->FetchSingle(request, level, this);

  // Step 2. Return from this algorithm, and run the following steps when
  // fetching a single module script asynchronously completes with result.
  // Note: Modulator::FetchSingle asynchronously notifies result to
  // ModuleTreeLinker::notifyModuleLoadFinished().
}

void ModuleTreeLinker::NotifyModuleLoadFinished(ModuleScript* module_script) {
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure

  // Step 3. "If result is null, ..."
  if (!module_script) {
    // "asynchronously complete this algorithm with null and abort these steps."
    // Note: We return null by calling AdvanceState(), which calls
    // NotifyModuleTreeLoadFinished(descendants_module_script_), and in this
    // case |descendants_module_script_| is always null.
    DCHECK(!descendants_module_script_);
    AdvanceState(State::kFinished);
    return;
  }

  // Step 4. Otherwise, result is a module script. Fetch the descendants of
  // result given destination and an ancestor list obtained by appending url to
  // ancestor list. Wait for fetching the descendants of a module script to
  // asynchronously complete with descendants result before proceeding to the
  // next step.
  module_script_ = module_script;

  FetchDescendants();

  // Note: Step 5- continues in Instantiate() method, after
  // "fetch the descendants of a module script" procedure completes.
}

class ModuleTreeLinker::DependencyModuleClient
    : public GarbageCollectedFinalized<
          ModuleTreeLinker::DependencyModuleClient>,
      public ModuleTreeClient {
  USING_GARBAGE_COLLECTED_MIXIN(ModuleTreeLinker::DependencyModuleClient);

 public:
  static DependencyModuleClient* Create(ModuleTreeLinker* module_tree_linker) {
    return new DependencyModuleClient(module_tree_linker);
  }
  virtual ~DependencyModuleClient() = default;

  DEFINE_INLINE_TRACE() {
    visitor->Trace(module_tree_linker_);
    ModuleTreeClient::Trace(visitor);
  }

 private:
  explicit DependencyModuleClient(ModuleTreeLinker* module_tree_linker)
      : module_tree_linker_(module_tree_linker) {
    CHECK(module_tree_linker);
  }

  // Implements ModuleTreeClient
  void NotifyModuleTreeLoadFinished(ModuleScript*) override;

  Member<ModuleTreeLinker> module_tree_linker_;
};

void ModuleTreeLinker::FetchDescendants() {
  CHECK(module_script_);
  AdvanceState(State::kFetchingDependencies);

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-a-module-script

  // Step 1. Let record be module script's module record.
  ScriptModule record = module_script_->Record();

  // Step 2. If record.[[RequestedModules]] is empty, asynchronously complete
  // this algorithm with module script.
  Vector<String> module_requests =
      modulator_->ModuleRequestsFromScriptModule(record);
  if (module_requests.IsEmpty()) {
    // Continue to Instantiate() to process "internal module script graph
    // fetching procedure" Step 5-.
    descendants_module_script_ = module_script_;
    Instantiate();
    return;
  }

  // Step 3. Let urls be a new empty list.
  Vector<KURL> urls;

  // Step 4. For each string requested of record.[[RequestedModules]],
  for (const auto& module_request : module_requests) {
    // Step 4.1. Let url be the result of resolving a module specifier given
    // module script and requested.
    KURL url = Modulator::ResolveModuleSpecifier(module_request,
                                                 module_script_->BaseURL());

    // Step 4.2. If the result is error: ...
    if (url.IsNull()) {
      // Let error be a new TypeError exception.
      // Report the exception error for module script.
      // TODO(kouhei): Implement the exception.

      // Abort this algorithm, and asynchronously complete it with null.
      // Note: The return variable for "internal module script graph fetching
      // procedure" is descendants_module_script_ per Step 8.
      DCHECK(!descendants_module_script_);
      // Note: while we complete "fetch the descendants of a module script"
      //       algorithm here, we still need to continue to the rest of the
      //       steps in "internal module script graph fetching procedure"
      Instantiate();
      return;
    }

    // Step 4.3. Otherwise, if ancestor list does not contain url, append url to
    // urls.

    // Modulator::resolveModuleSpecifier() impl must return either a valid url
    // or null.
    CHECK(url.IsValid());
    if (!ancestor_list_with_url_.Contains(url))
      urls.push_back(url);
  }

  // Step 5. For each url in urls, perform the internal module script graph
  // fetching procedure given url, module script's credentials mode, module
  // script's cryptographic nonce, module script's parser state, destination,
  // module script's settings object, module script's settings object, ancestor
  // list, module script's base URL, and with the top-level module fetch flag
  // unset. If the caller of this algorithm specified custom perform the fetch
  // steps, pass those along while performing the internal module script graph
  // fetching procedure.
  // TODO(kouhei): handle "destination".
  DCHECK(!urls.IsEmpty());
  CHECK_EQ(num_incomplete_descendants_, 0u);
  num_incomplete_descendants_ = urls.size();
  for (const KURL& url : urls) {
    DependencyModuleClient* dependency_client =
        DependencyModuleClient::Create(this);
    dependency_clients_.insert(dependency_client);

    ModuleScriptFetchRequest request(url, module_script_->Nonce(),
                                     module_script_->ParserState(),
                                     module_script_->CredentialsMode(),
                                     module_script_->BaseURL().GetString());
    modulator_->FetchTreeInternal(request, ancestor_list_with_url_,
                                  ModuleGraphLevel::kDependentModuleFetch,
                                  dependency_client);
  }

  // Asynchronously continue processing after notifyOneDescendantFinished() is
  // called m_numIncompleteDescendants times.
  CHECK_GT(num_incomplete_descendants_, 0u);
}

void ModuleTreeLinker::DependencyModuleClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  DescendantLoad was_success =
      module_script ? DescendantLoad::kSuccess : DescendantLoad::kFailed;
  module_tree_linker_->NotifyOneDescendantFinished(was_success);
}

void ModuleTreeLinker::NotifyOneDescendantFinished(DescendantLoad was_success) {
  CHECK(!descendants_module_script_);

  if (state_ == State::kFinished) {
    // We may reach here if one of the descendant failed to load, and the other
    // descendants fetches were in flight.
    return;
  }
  CHECK_EQ(state_, State::kFetchingDependencies);

  CHECK_GT(num_incomplete_descendants_, 0u);
  --num_incomplete_descendants_;

  // TODO(kouhei): Potential room for optimization. Cancel inflight descendants
  // fetch if a descendant load failed.

  CHECK(module_script_);
  RESOURCE_LOADING_DVLOG(1)
      << "ModuleTreeLinker[" << this << "]::NotifyOneDescendantFinished with "
      << (was_success == DescendantLoad::kSuccess ? "success. " : "failure. ")
      << num_incomplete_descendants_ << " remaining descendants.";

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-a-module-script
  // Step 5. "... If any of them asynchronously complete with null, then
  // asynchronously complete this algorithm with null ..."
  if (was_success == DescendantLoad::kFailed) {
    DCHECK(!descendants_module_script_);
    // Note: while we complete "fetch the descendants of a module script"
    //       algorithm here, we still need to continue to the rest of the steps
    //       in "internal module script graph fetching procedure"
    Instantiate();
    return;
  }

  // Step 5. "Wait for all of the internal module script graph fetching
  // procedure invocations to asynchronously complete. ... Otherwise,
  // asynchronously complete this algorithm with module script."
  if (!num_incomplete_descendants_) {
    descendants_module_script_ = module_script_;
    Instantiate();
  }
}

void ModuleTreeLinker::Instantiate() {
  CHECK(module_script_);
  AdvanceState(State::kInstantiating);

  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure

  // Step 5. Let record be result's module record.
  ScriptModule record = module_script_->Record();

  // Step 6. Let instantiationStatus be record.ModuleDeclarationInstantiation().
  // Note: The |error| variable corresponds to spec variable
  // "instantiationStatus". If |error| is empty, it indicates successful
  // completion.
  ScriptValue error = modulator_->InstantiateModule(record);

  // Step 7. For each module script script in result's uninstantiated inclusive
  // descendant module scripts, perform the following steps:
  HeapHashSet<Member<ModuleScript>> uninstantiated_set =
      UninstantiatedInclusiveDescendants();
  for (const auto& descendant : uninstantiated_set) {
    if (!error.IsEmpty()) {
      // Step 7.1. If instantiationStatus is an abrupt completion, then set
      // script's instantiation state to "errored", its instantiation error to
      // instantiationStatus.[[Value]], and its module record to null.
      descendant->SetInstantiationErrorAndClearRecord(error);
    } else {
      // Step 7.2. Otherwise, set script's instantiation state to
      // "instantiated".
      descendant->SetInstantiationSuccess();
    }
  }

  // Step 8. Asynchronously complete this algorithm with descendants result.
  AdvanceState(State::kFinished);
}

HeapHashSet<Member<ModuleScript>>
ModuleTreeLinker::UninstantiatedInclusiveDescendants() {
  // https://html.spec.whatwg.org/multipage/webappapis.html#uninstantiated-inclusive-descendant-module-scripts
  // Step 1. Let moduleMap be script's settings object's module map.
  // Note: Modulator is our "settings object".
  // Note: We won't reference the ModuleMap directly here to aid testing.

  // Step 2. Let stack be the stack << script >>.
  // TODO(kouhei): Make stack a HeapLinkedHashSet for O(1) lookups.
  HeapDeque<Member<ModuleScript>> stack;
  stack.push_front(module_script_);

  // Step 3. Let inclusive descendants be an empty set.
  // Note: We use unordered set here as the order is not observable from web
  // platform.
  // This is allowed per spec: https://infra.spec.whatwg.org/#sets
  HeapHashSet<Member<ModuleScript>> inclusive_descendants;

  // Step 4. While stack is not empty:
  while (!stack.IsEmpty()) {
    // Step 4.1. Let current the result of popping from stack.
    ModuleScript* current = stack.TakeFirst();

    // Step 4.2. Assert: current is a module script (i.e., it is not "fetching"
    // or null).
    DCHECK(current);

    // Step 4.3. If inclusive descendants and stack both do not contain current,
    // then:
    if (inclusive_descendants.Contains(current))
      continue;
    if (std::find(stack.begin(), stack.end(), current) != stack.end())
      continue;

    // Step 4.3.1. Append current to inclusive descendants.
    inclusive_descendants.insert(current);

    // TODO(kouhei): This implementation is a direct transliteration of the
    // spec. Omit intermediate vectors at the least.

    // Step 4.3.2. Let child specifiers be the value of current's module
    // record's [[RequestedModules]] internal slot.
    Vector<String> child_specifiers =
        modulator_->ModuleRequestsFromScriptModule(current->Record());
    // Step 4.3.3. Let child URLs be the list obtained by calling resolve a
    // module specifier once for each item of child specifiers, given current
    // and that item. Omit any failures.
    Vector<KURL> child_urls;
    for (const auto& child_specifier : child_specifiers) {
      KURL child_url = modulator_->ResolveModuleSpecifier(child_specifier,
                                                          current->BaseURL());
      if (child_url.IsValid())
        child_urls.push_back(child_url);
    }
    // Step 4.3.4. Let child modules be the list obtained by getting each value
    // in moduleMap whose key is given by an item of child URLs.
    HeapVector<Member<ModuleScript>> child_modules;
    for (const auto& child_url : child_urls) {
      ModuleScript* module_script =
          modulator_->GetFetchedModuleScript(child_url);

      child_modules.push_back(module_script);
    }
    // Step 4.3.5. For each s of child modules:
    for (const auto& s : child_modules) {
      // Step 4.3.5.2. If s is null, continue.
      // Note: We do null check first, as Blink HashSet can't contain nullptr.

      // Step 4.3.5.3. Assert: s is a module script (i.e., it is not "fetching",
      // since by this point all child modules must have been fetched). Note:
      // GetFetchedModuleScript returns nullptr if "fetching"
      if (!s)
        continue;

      // Step 4.3.5.1. If inclusive descendants already contains s, continue.
      if (inclusive_descendants.Contains(s))
        continue;

      // Step 4.3.5.4. Push s onto stack.
      stack.push_front(s);
    }
  }

  // Step 5. Return a set containing all items of inclusive descendants whose
  // instantiation state is "uninstantiated".
  HeapHashSet<Member<ModuleScript>> uninstantiated_set;
  for (const auto& script : inclusive_descendants) {
    if (script->InstantiationState() ==
        ModuleInstantiationState::kUninstantiated)
      uninstantiated_set.insert(script);
  }
  return uninstantiated_set;
}

}  // namespace blink
