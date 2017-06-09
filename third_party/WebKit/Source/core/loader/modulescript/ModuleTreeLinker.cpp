// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleTreeLinker.h"

#include "bindings/core/v8/ScriptModule.h"
#include "core/dom/AncestorList.h"
#include "core/dom/ModuleScript.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleTreeLinkerRegistry.h"
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
    ModuleTreeLinkerRegistry* registry,
    ModuleTreeClient* client) {
  AncestorList ancestor_list_with_url = ancestor_list;
  ancestor_list_with_url.insert(request.Url());

  ModuleTreeLinker* fetcher =
      new ModuleTreeLinker(ancestor_list_with_url, modulator, registry, client);
  fetcher->FetchSelf(request, level);
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
  ModuleTreeLinker* fetcher =
      new ModuleTreeLinker(empty_ancestor_list, modulator, registry, client);
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
            new_state == State::kFinished);
      break;
    case State::kFetchingDependencies:
      CHECK(new_state == State::kInstantiating ||
            new_state == State::kFinished);
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
    RESOURCE_LOADING_DVLOG(1)
        << "ModuleTreeLinker[" << this << "] finished with final result "
        << descendants_module_script_;

    registry_->ReleaseFinishedFetcher(this);

    // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure
    // Step 7. When the "fetch the descendants of and instantiate a module
    // script" algorithm asynchronously completes with final result,
    // asynchronously complete this algorithm with final result.
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
  // ModuleTreeLinker::NotifyModuleLoadFinished().
}

void ModuleTreeLinker::NotifyModuleLoadFinished(ModuleScript* result) {
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure

  // Step 3. "If result is null, ..."
  // Step 4. "If result's state is "instantiated" or "errored", ..."
  if (!result || result->State() == ModuleInstantiationState::kInstantiated ||
      result->State() == ModuleInstantiationState::kErrored) {
    // "asynchronously complete this algorithm with result, and abort these
    // steps."
    descendants_module_script_ = result;
    AdvanceState(State::kFinished);
    return;
  }

  // Step 5. Assert: result's state is "uninstantiated".
  DCHECK_EQ(ModuleInstantiationState::kUninstantiated, result->State());

  // Step 6. Fetch the descendants of and instantiate result given destination
  // and an ancestor list obtained by appending url to ancestor list.
  module_script_ = result;
  FetchDescendants();
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

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-and-instantiate-a-module-script
  // Step 1. If ancestor list was not given, let it be the empty list.
  // Note: The "ancestor list" in spec corresponds to |ancestor_list_with_url_|.

  // Step 2. If module script's state is "instantiated" or "errored",
  // asynchronously complete this algorithm with module script, and abort these
  // steps.
  if (module_script_->State() == ModuleInstantiationState::kInstantiated ||
      module_script_->State() == ModuleInstantiationState::kErrored) {
    descendants_module_script_ = module_script_;
    AdvanceState(State::kFinished);
    return;
  }

  // Step 3. Assert: module script's state is "uninstantiated"
  DCHECK_EQ(ModuleInstantiationState::kUninstantiated, module_script_->State());

  // Step 4. Let record be module script's module record.
  ScriptModule record = module_script_->Record();
  DCHECK(!record.IsNull());

  // Step 5. If record.[[RequestedModules]] is empty, asynchronously complete
  // this algorithm with module script.
  // Note: We defer this bail-out until Step 9. Step 6-9 will be no-op anyway if
  //       record.[[RequestedModules]] is empty.

  // Step 6. Let urls be a new empty list.
  Vector<KURL> urls;

  // Step 7. For each string requested of record.[[RequestedModules]],
  Vector<String> module_requests =
      modulator_->ModuleRequestsFromScriptModule(record);
  for (const auto& module_request : module_requests) {
    // Step 7.1. Let url be the result of resolving a module specifier given
    // module script and requested.
    KURL url = Modulator::ResolveModuleSpecifier(module_request,
                                                 module_script_->BaseURL());

    // Step 7.2. If url is failure: ...
    if (url.IsNull()) {
      // Step 7.2.1. Let error be a new TypeError exception.
      ScriptState::Scope scope(modulator_->GetScriptState());
      v8::Isolate* isolate = modulator_->GetScriptState()->GetIsolate();
      v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
          isolate,
          "Failed to resolve module specifier \'" + module_request + "'");

      // Step 7.2.2. Error module script with error.
      module_script_->SetErrorAndClearRecord(
          ScriptValue(modulator_->GetScriptState(), error));

      // Step 7.2.3. Abort this algorithm, and asynchronously complete it with
      // module_sript_. Note: The return variable for "internal module script
      // graph fetching procedure" is descendants_module_script_ per Step 7.
      descendants_module_script_ = module_script_;
      AdvanceState(State::kFinished);
      return;
    }

    // Step 7.3. Otherwise, if ancestor list does not contain url, append url to
    // urls.
    CHECK(url.IsValid()) << "Modulator::resolveModuleSpecifier() impl must "
                            "return either a valid url or null.";
    if (!ancestor_list_with_url_.Contains(url))
      urls.push_back(url);
  }

  // Step 8. Let descendants result be null.
  DCHECK(!descendants_module_script_);

  // Step 9. For each url in urls, perform the internal module script graph
  // fetching procedure given url, module script's credentials mode, module
  // script's cryptographic nonce, module script's parser state, destination,
  // module script's settings object, module script's settings object, ancestor
  // list, module script's base URL, and with the top-level module fetch flag
  // unset. If the caller of this algorithm specified custom perform the fetch
  // steps, pass those along while performing the internal module script graph
  // fetching procedure.

  if (urls.IsEmpty()) {
    // Step 5. If record.[[RequestedModules]] is empty, asynchronously
    // complete this algorithm with module script. [spec text]
    // Note: We actually proceed to Step 10 here. [nospec, but a pending PR
    // fixes this.]

    // Also, if record.[[RequestedModules]] is not empty but |urls| is
    // empty here, we can proceed to Step 10.
    descendants_module_script_ = module_script_;
    Instantiate();
    return;
  }

  // Step 9, when "urls" is non-empty.
  // These invocations of the internal module script graph fetching procedure
  // should be performed in parallel to each other. [spec text]
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

  // Asynchronously continue processing after NotifyOneDescendantFinished() is
  // called num_incomplete_descendants_ times.
  CHECK_GT(num_incomplete_descendants_, 0u);
}

void ModuleTreeLinker::DependencyModuleClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  module_tree_linker_->NotifyOneDescendantFinished(module_script);
}

void ModuleTreeLinker::NotifyOneDescendantFinished(
    ModuleScript* module_script) {
  if (state_ == State::kFinished) {
    // We may reach here if one of the descendant failed to load, and the other
    // descendants fetches were in flight.
    return;
  }
  DCHECK_EQ(state_, State::kFetchingDependencies);
  DCHECK(module_script_);

  CHECK_GT(num_incomplete_descendants_, 0u);
  --num_incomplete_descendants_;

  // Step 9 of
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-and-instantiate-a-module-script
  // "If any invocation of the internal module script graph fetching procedure
  // asynchronously completes with null, then ..." [spec text]
  if (!module_script) {
    RESOURCE_LOADING_DVLOG(1)
        << "ModuleTreeLinker[" << this
        << "]::NotifyOneDescendantFinished with null. "
        << num_incomplete_descendants_ << " remaining descendants.";

    // "optionally abort all other invocations, " [spec text]
    // TODO(kouhei) Implement this.

    // "set descendants result to null, and ... " [spec text]
    DCHECK(!descendants_module_script_);

    // "proceed to the next step. (The un-fetched descendant will cause errors
    // during instantiation.)" [spec text]
    Instantiate();
    return;
  }

  RESOURCE_LOADING_DVLOG(1)
      << "ModuleTreeLinker[" << this
      << "]::NotifyOneDescendantFinished with a module script of state \""
      << ModuleInstantiationStateToString(module_script->State()) << "\". "
      << num_incomplete_descendants_ << " remaining descendants.";

  // "If any invocation of the internal module script graph fetching procedure
  // asynchronously completes with a module script whose state is "errored",
  // then ..." [spec text]
  if (module_script->State() == ModuleInstantiationState::kErrored) {
    // "optionally abort all other invocations, ..." [spec text]
    // TODO(kouhei) Implement this.

    // "set descendants result to module script, ..." [spec text]
    descendants_module_script_ = module_script_;

    // "and proceed to the next step. (The errored descendant will cause errors
    // during instantiation.)" [spec text]
    Instantiate();
    return;
  }

  // "Otherwise, wait for all of the internal module script graph fetching
  // procedure invocations to asynchronously complete, with module scripts whose
  // states are not "errored"." [spec text]
  if (!num_incomplete_descendants_) {
    // "Then, set descendants result to module script, and proceed to the next
    // step." [spec text]
    descendants_module_script_ = module_script_;
    Instantiate();
  }
}

void ModuleTreeLinker::Instantiate() {
  CHECK(module_script_);
  AdvanceState(State::kInstantiating);

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-and-instantiate-a-module-script

  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    descendants_module_script_ = nullptr;
    AdvanceState(State::kFinished);
    return;
  }

  // Contrary to the spec, we don't store the "record" in Step 4 for its use in
  // Step 10. If Instantiate() was called on descendant ModuleTreeLinker and
  // failed, module_script_->Record() may be already cleared.
  if (module_script_->State() == ModuleInstantiationState::kErrored) {
    DCHECK(module_script_->HasEmptyRecord());
    AdvanceState(State::kFinished);
    return;
  }

  // Step 4. Let record be result's module record.
  ScriptModule record = module_script_->Record();

  // Step 10. Let instantiationStatus be record.ModuleDeclarationInstantiation.
  // Note: The |error| variable corresponds to spec variable
  // "instantiationStatus". If |error| is empty, it indicates successful
  // completion.
  ScriptValue error = modulator_->InstantiateModule(record);

  // Step 11. For each script in module script's uninstantiated inclusive
  // descendant module scripts, perform the following steps:
  HeapHashSet<Member<ModuleScript>> uninstantiated_set =
      UninstantiatedInclusiveDescendants();
  for (const auto& descendant : uninstantiated_set) {
    if (!error.IsEmpty()) {
      // Step 11.1. If instantiationStatus is an abrupt completion, then
      // error script with instantiationStatus.[[Value]]
      descendant->SetErrorAndClearRecord(error);
    } else {
      // Step 11.2. Otherwise, set script's instantiation state to
      // "instantiated".
      descendant->SetInstantiationSuccess();
    }
  }

  // Step 12. Asynchronously complete this algorithm with descendants result.
  AdvanceState(State::kFinished);
}

HeapHashSet<Member<ModuleScript>>
ModuleTreeLinker::UninstantiatedInclusiveDescendants() {
  // https://html.spec.whatwg.org/multipage/webappapis.html#uninstantiated-inclusive-descendant-module-scripts
  // Step 1. If script's module record is null, return the empty set.
  if (module_script_->HasEmptyRecord())
    return HeapHashSet<Member<ModuleScript>>();

  // Step 2. Let moduleMap be script's settings object's module map.
  // Note: Modulator is our "settings object".
  // Note: We won't reference the ModuleMap directly here to aid testing.

  // Step 3. Let stack be the stack << script >>.
  // TODO(kouhei): Make stack a HeapLinkedHashSet for O(1) lookups.
  HeapDeque<Member<ModuleScript>> stack;
  stack.push_front(module_script_);

  // Step 4. Let inclusive descendants be an empty set.
  // Note: We use unordered set here as the order is not observable from web
  // platform.
  // This is allowed per spec: https://infra.spec.whatwg.org/#sets
  HeapHashSet<Member<ModuleScript>> inclusive_descendants;

  // Step 5. While stack is not empty:
  while (!stack.IsEmpty()) {
    // Step 5.1. Let current the result of popping from stack.
    ModuleScript* current = stack.TakeFirst();

    // Step 5.2. Assert: current is a module script (i.e., it is not "fetching"
    // or null).
    DCHECK(current);

    // Step 5.3. If inclusive descendants and stack both do not contain current,
    // then:
    if (inclusive_descendants.Contains(current))
      continue;
    if (std::find(stack.begin(), stack.end(), current) != stack.end())
      continue;

    // Step 5.3.1. Append current to inclusive descendants.
    inclusive_descendants.insert(current);

    // TODO(kouhei): This implementation is a direct transliteration of the
    // spec. Omit intermediate vectors at the least.

    // Step 5.3.2. Let child specifiers be the value of current's module
    // record's [[RequestedModules]] internal slot.
    Vector<String> child_specifiers =
        modulator_->ModuleRequestsFromScriptModule(current->Record());
    // Step 5.3.3. Let child URLs be the list obtained by calling resolve a
    // module specifier once for each item of child specifiers, given current
    // and that item. Omit any failures.
    Vector<KURL> child_urls;
    for (const auto& child_specifier : child_specifiers) {
      KURL child_url = modulator_->ResolveModuleSpecifier(child_specifier,
                                                          current->BaseURL());
      if (child_url.IsValid())
        child_urls.push_back(child_url);
    }
    // Step 5.3.4. Let child modules be the list obtained by getting each value
    // in moduleMap whose key is given by an item of child URLs.
    HeapVector<Member<ModuleScript>> child_modules;
    for (const auto& child_url : child_urls) {
      ModuleScript* module_script =
          modulator_->GetFetchedModuleScript(child_url);

      child_modules.push_back(module_script);
    }
    // Step 5.3.5. For each s of child modules:
    for (const auto& s : child_modules) {
      // Step 5.3.5.2. If s is null, continue.
      // Note: We do null check first, as Blink HashSet can't contain nullptr.

      // Step 5.3.5.3. Assert: s is a module script (i.e., it is not "fetching",
      // since by this point all child modules must have been fetched). Note:
      // GetFetchedModuleScript returns nullptr if "fetching"
      if (!s)
        continue;

      // Step 5.3.5.1. If inclusive descendants already contains s, continue.
      if (inclusive_descendants.Contains(s))
        continue;

      // Step 5.3.5.4. Push s onto stack.
      stack.push_front(s);
    }
  }

  // Step 6. Return a set containing all items of inclusive descendants whose
  // instantiation state is "uninstantiated".
  HeapHashSet<Member<ModuleScript>> uninstantiated_set;
  for (const auto& script : inclusive_descendants) {
    if (script->State() == ModuleInstantiationState::kUninstantiated)
      uninstantiated_set.insert(script);
  }
  return uninstantiated_set;
}

}  // namespace blink
