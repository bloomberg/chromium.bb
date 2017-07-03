// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleTreeLinker.h"

#include "bindings/core/v8/ScriptModule.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/Modulator.h"
#include "core/dom/ModuleScript.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleTreeLinkerRegistry.h"
#include "core/testing/DummyModulator.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/Platform.h"
#include "public/platform/scheduler/renderer/renderer_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class TestModuleTreeClient final : public ModuleTreeClient {
 public:
  TestModuleTreeClient() = default;

  DEFINE_INLINE_TRACE() {
    visitor->Trace(module_script_);
    ModuleTreeClient::Trace(visitor);
  }

  void NotifyModuleTreeLoadFinished(ModuleScript* module_script) override {
    was_notify_finished_ = true;
    module_script_ = module_script;
  }

  bool WasNotifyFinished() const { return was_notify_finished_; }
  ModuleScript* GetModuleScript() { return module_script_; }

 private:
  bool was_notify_finished_ = false;
  Member<ModuleScript> module_script_;
};

}  // namespace

class ModuleTreeLinkerTestModulator final : public DummyModulator {
 public:
  ModuleTreeLinkerTestModulator(RefPtr<ScriptState> script_state)
      : script_state_(std::move(script_state)) {}
  ~ModuleTreeLinkerTestModulator() override {}

  DECLARE_TRACE();

  enum class ResolveResult { kFailure, kSuccess };

  // Resolve last |Modulator::FetchSingle()| call.
  ModuleScript* ResolveSingleModuleScriptFetch(
      const KURL& url,
      const Vector<String>& dependency_module_specifiers,
      ModuleInstantiationState state) {
    ScriptState::Scope scope(script_state_.Get());

    StringBuilder source_text;
    Vector<ModuleRequest> dependency_module_requests;
    dependency_module_requests.ReserveInitialCapacity(
        dependency_module_specifiers.size());
    for (const auto& specifier : dependency_module_specifiers) {
      dependency_module_requests.emplace_back(specifier,
                                              TextPosition::MinimumPosition());
      source_text.Append("import '");
      source_text.Append(specifier);
      source_text.Append("';\n");
    }
    source_text.Append("export default 'grapes';");

    ScriptModule script_module = ScriptModule::Compile(
        script_state_->GetIsolate(), source_text.ToString(), url.GetString(),
        kSharableCrossOrigin, TextPosition::MinimumPosition(),
        ASSERT_NO_EXCEPTION);
    ModuleScript* module_script = ModuleScript::CreateForTest(
        this, script_module, url, "", kParserInserted,
        WebURLRequest::kFetchCredentialsModeOmit);
    auto result_request = dependency_module_requests_map_.insert(
        script_module, dependency_module_requests);
    EXPECT_TRUE(result_request.is_new_entry);
    auto result_map = module_map_.insert(url, module_script);
    EXPECT_TRUE(result_map.is_new_entry);

    if (state == ModuleInstantiationState::kErrored) {
      v8::Local<v8::Value> error = V8ThrowException::CreateError(
          script_state_->GetIsolate(), "Instantiation failure.");
      module_script->SetErrorAndClearRecord(
          ScriptValue(script_state_.Get(), error));
    }

    EXPECT_EQ(url, pending_request_url_);
    if (state == ModuleInstantiationState::kErrored) {
      EXPECT_TRUE(module_script->IsErrored());
    } else {
      EXPECT_EQ(state, module_script->State());
    }
    EXPECT_TRUE(pending_client_);
    pending_client_->NotifyModuleLoadFinished(module_script);
    pending_client_.Clear();

    return module_script;
  }

  // Get AncestorList specified in |Modulator::FetchTreeInternal()| call for
  // request matching |url|.
  AncestorList GetAncestorListForTreeFetch(const KURL& url) const {
    const auto& it = pending_tree_ancestor_list_.find(url);
    if (it == pending_tree_ancestor_list_.end())
      return AncestorList();
    return it->value;
  }

  // Resolve |Modulator::FetchTreeInternal()| for given url.
  void ResolveDependentTreeFetch(const KURL& url, ResolveResult result) {
    const auto& it = pending_tree_client_map_.find(url);
    EXPECT_NE(pending_tree_client_map_.end(), it);
    auto pending_client = it->value;
    EXPECT_TRUE(pending_client);
    pending_tree_client_map_.erase(it);

    if (result == ResolveResult::kFailure) {
      pending_client->NotifyModuleTreeLoadFinished(nullptr);
      return;
    }
    EXPECT_EQ(ResolveResult::kSuccess, result);

    ScriptState::Scope scope(script_state_.Get());

    ScriptModule script_module = ScriptModule::Compile(
        script_state_->GetIsolate(), "export default 'pineapples';",
        url.GetString(), kSharableCrossOrigin, TextPosition::MinimumPosition(),
        ASSERT_NO_EXCEPTION);
    ModuleScript* module_script = ModuleScript::CreateForTest(
        this, script_module, url, "", kParserInserted,
        WebURLRequest::kFetchCredentialsModeOmit);
    auto result_map = module_map_.insert(url, module_script);
    EXPECT_TRUE(result_map.is_new_entry);

    pending_client->NotifyModuleTreeLoadFinished(module_script);
  }

  void SetInstantiateShouldFail(bool b) { instantiate_should_fail_ = b; }

 private:
  // Implements Modulator:

  ScriptState* GetScriptState() override { return script_state_.Get(); }

  void FetchSingle(const ModuleScriptFetchRequest& request,
                   ModuleGraphLevel,
                   SingleModuleClient* client) override {
    pending_request_url_ = request.Url();
    EXPECT_FALSE(pending_client_);
    pending_client_ = client;
  }

  void FetchTreeInternal(const ModuleScriptFetchRequest& request,
                         const AncestorList& list,
                         ModuleGraphLevel level,
                         ModuleTreeClient* client) override {
    const auto& url = request.Url();

    auto ancestor_result = pending_tree_ancestor_list_.insert(url, list);
    EXPECT_TRUE(ancestor_result.is_new_entry);

    EXPECT_EQ(ModuleGraphLevel::kDependentModuleFetch, level);

    auto result_map = pending_tree_client_map_.insert(url, client);
    EXPECT_TRUE(result_map.is_new_entry);
  }

  ModuleScript* GetFetchedModuleScript(const KURL& url) override {
    const auto& it = module_map_.find(url);
    if (it == module_map_.end())
      return nullptr;

    return it->value;
  }

  ScriptValue InstantiateModule(ScriptModule record) override {
    if (instantiate_should_fail_) {
      ScriptState::Scope scope(script_state_.Get());
      v8::Local<v8::Value> error = V8ThrowException::CreateError(
          script_state_->GetIsolate(), "Instantiation failure.");
      errored_records_.insert(record);
      return ScriptValue(script_state_.Get(), error);
    }
    instantiated_records_.insert(record);
    return ScriptValue();
  }

  ScriptModuleState GetRecordStatus(ScriptModule record) override {
    if (instantiated_records_.Contains(record))
      return ScriptModuleState::kInstantiated;
    if (errored_records_.Contains(record))
      return ScriptModuleState::kErrored;
    return ScriptModuleState::kUninstantiated;
  }

  ScriptValue GetError(const ModuleScript* module_script) override {
    ScriptState::Scope scope(script_state_.Get());
    return ScriptValue(script_state_.Get(), module_script->CreateErrorInternal(
                                                script_state_->GetIsolate()));
  }

  Vector<ModuleRequest> ModuleRequestsFromScriptModule(
      ScriptModule script_module) override {
    if (script_module.IsNull())
      return Vector<ModuleRequest>();

    const auto& it = dependency_module_requests_map_.find(script_module);
    if (it == dependency_module_requests_map_.end())
      return Vector<ModuleRequest>();

    return it->value;
  }

  RefPtr<ScriptState> script_state_;
  KURL pending_request_url_;
  Member<SingleModuleClient> pending_client_;
  HashMap<ScriptModule, Vector<ModuleRequest>> dependency_module_requests_map_;
  HeapHashMap<KURL, Member<ModuleScript>> module_map_;
  HeapHashMap<KURL, Member<ModuleTreeClient>> pending_tree_client_map_;
  HashMap<KURL, AncestorList> pending_tree_ancestor_list_;
  HashSet<ScriptModule> instantiated_records_;
  HashSet<ScriptModule> errored_records_;
  bool instantiate_should_fail_ = false;
};

DEFINE_TRACE(ModuleTreeLinkerTestModulator) {
  visitor->Trace(pending_client_);
  visitor->Trace(module_map_);
  visitor->Trace(pending_tree_client_map_);
  DummyModulator::Trace(visitor);
}

class ModuleTreeLinkerTest : public ::testing::Test {
  DISALLOW_COPY_AND_ASSIGN(ModuleTreeLinkerTest);

 public:
  ModuleTreeLinkerTest() = default;
  void SetUp() override;

  ModuleTreeLinkerTestModulator* GetModulator() { return modulator_.Get(); }

 protected:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<ModuleTreeLinkerTestModulator> modulator_;
};

void ModuleTreeLinkerTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(500, 500));
  RefPtr<ScriptState> script_state =
      ToScriptStateForMainWorld(&dummy_page_holder_->GetFrame());
  modulator_ = new ModuleTreeLinkerTestModulator(script_state);
}

TEST_F(ModuleTreeLinkerTest, FetchTreeNoDeps) {
  ModuleTreeLinkerRegistry* registry = ModuleTreeLinkerRegistry::Create();

  KURL url(kParsedURLString, "http://example.com/root.js");
  ModuleScriptFetchRequest module_request(
      url, String(), kParserInserted, WebURLRequest::kFetchCredentialsModeOmit);
  TestModuleTreeClient* client = new TestModuleTreeClient;
  registry->Fetch(module_request, AncestorList(),
                  ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(),
                  client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(
      url, {}, ModuleInstantiationState::kUninstantiated);
  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_EQ(client->GetModuleScript()->State(),
            ModuleInstantiationState::kInstantiated);
}

TEST_F(ModuleTreeLinkerTest, FetchTreeInstantiationFailure) {
  GetModulator()->SetInstantiateShouldFail(true);

  ModuleTreeLinkerRegistry* registry = ModuleTreeLinkerRegistry::Create();

  KURL url(kParsedURLString, "http://example.com/root.js");
  ModuleScriptFetchRequest module_request(
      url, String(), kParserInserted, WebURLRequest::kFetchCredentialsModeOmit);
  TestModuleTreeClient* client = new TestModuleTreeClient;
  registry->Fetch(module_request, AncestorList(),
                  ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(),
                  client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(
      url, {}, ModuleInstantiationState::kUninstantiated);

  // Modulator::InstantiateModule() fails here, as
  // we SetInstantiateShouldFail(true) earlier.

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client->GetModuleScript()->IsErrored());
}

TEST_F(ModuleTreeLinkerTest, FetchTreePreviousInstantiationFailure) {
  ModuleTreeLinkerRegistry* registry = ModuleTreeLinkerRegistry::Create();

  KURL url(kParsedURLString, "http://example.com/root.js");
  ModuleScriptFetchRequest module_request(
      url, String(), kParserInserted, WebURLRequest::kFetchCredentialsModeOmit);
  TestModuleTreeClient* client = new TestModuleTreeClient;
  registry->Fetch(module_request, AncestorList(),
                  ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(),
                  client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  // This emulates "previous instantiation failure", where
  // Modulator::FetchSingle resolves w/ "errored" module script.
  GetModulator()->ResolveSingleModuleScriptFetch(
      url, {}, ModuleInstantiationState::kErrored);
  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client->GetModuleScript()->IsErrored());
}

TEST_F(ModuleTreeLinkerTest, FetchTreeWithSingleDependency) {
  ModuleTreeLinkerRegistry* registry = ModuleTreeLinkerRegistry::Create();

  KURL url(kParsedURLString, "http://example.com/root.js");
  ModuleScriptFetchRequest module_request(
      url, String(), kParserInserted, WebURLRequest::kFetchCredentialsModeOmit);
  TestModuleTreeClient* client = new TestModuleTreeClient;
  registry->Fetch(module_request, AncestorList(),
                  ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(),
                  client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(
      url, {"./dep1.js"}, ModuleInstantiationState::kUninstantiated);
  EXPECT_FALSE(client->WasNotifyFinished());

  KURL url_dep1(kParsedURLString, "http://example.com/dep1.js");
  auto ancestor_list = GetModulator()->GetAncestorListForTreeFetch(url_dep1);
  EXPECT_EQ(1u, ancestor_list.size());
  EXPECT_TRUE(ancestor_list.Contains(
      KURL(kParsedURLString, "http://example.com/root.js")));

  GetModulator()->ResolveDependentTreeFetch(
      url_dep1, ModuleTreeLinkerTestModulator::ResolveResult::kSuccess);
  EXPECT_TRUE(client->WasNotifyFinished());

  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_EQ(client->GetModuleScript()->State(),
            ModuleInstantiationState::kInstantiated);
}

TEST_F(ModuleTreeLinkerTest, FetchTreeWith3Deps) {
  ModuleTreeLinkerRegistry* registry = ModuleTreeLinkerRegistry::Create();

  KURL url(kParsedURLString, "http://example.com/root.js");
  ModuleScriptFetchRequest module_request(
      url, String(), kParserInserted, WebURLRequest::kFetchCredentialsModeOmit);
  TestModuleTreeClient* client = new TestModuleTreeClient;
  registry->Fetch(module_request, AncestorList(),
                  ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(),
                  client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(
      url, {"./dep1.js", "./dep2.js", "./dep3.js"},
      ModuleInstantiationState::kUninstantiated);
  EXPECT_FALSE(client->WasNotifyFinished());

  Vector<KURL> url_deps;
  for (int i = 1; i <= 3; ++i) {
    StringBuilder url_dep_str;
    url_dep_str.Append("http://example.com/dep");
    url_dep_str.AppendNumber(i);
    url_dep_str.Append(".js");

    KURL url_dep(kParsedURLString, url_dep_str.ToString());
    url_deps.push_back(url_dep);
  }

  for (const auto& url_dep : url_deps) {
    SCOPED_TRACE(url_dep.GetString());
    auto ancestor_list = GetModulator()->GetAncestorListForTreeFetch(url_dep);
    EXPECT_EQ(1u, ancestor_list.size());
    EXPECT_TRUE(ancestor_list.Contains(
        KURL(kParsedURLString, "http://example.com/root.js")));
  }

  for (const auto& url_dep : url_deps) {
    EXPECT_FALSE(client->WasNotifyFinished());
    GetModulator()->ResolveDependentTreeFetch(
        url_dep, ModuleTreeLinkerTestModulator::ResolveResult::kSuccess);
  }

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_EQ(client->GetModuleScript()->State(),
            ModuleInstantiationState::kInstantiated);
}

TEST_F(ModuleTreeLinkerTest, FetchTreeWith3Deps1Fail) {
  ModuleTreeLinkerRegistry* registry = ModuleTreeLinkerRegistry::Create();

  KURL url(kParsedURLString, "http://example.com/root.js");
  ModuleScriptFetchRequest module_request(
      url, String(), kParserInserted, WebURLRequest::kFetchCredentialsModeOmit);
  TestModuleTreeClient* client = new TestModuleTreeClient;
  registry->Fetch(module_request, AncestorList(),
                  ModuleGraphLevel::kTopLevelModuleFetch, GetModulator(),
                  client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(
      url, {"./dep1.js", "./dep2.js", "./dep3.js"},
      ModuleInstantiationState::kUninstantiated);
  EXPECT_FALSE(client->WasNotifyFinished());

  Vector<KURL> url_deps;
  for (int i = 1; i <= 3; ++i) {
    StringBuilder url_dep_str;
    url_dep_str.Append("http://example.com/dep");
    url_dep_str.AppendNumber(i);
    url_dep_str.Append(".js");

    KURL url_dep(kParsedURLString, url_dep_str.ToString());
    url_deps.push_back(url_dep);
  }

  for (const auto& url_dep : url_deps) {
    SCOPED_TRACE(url_dep.GetString());
    auto ancestor_list = GetModulator()->GetAncestorListForTreeFetch(url_dep);
    EXPECT_EQ(1u, ancestor_list.size());
    EXPECT_TRUE(ancestor_list.Contains(
        KURL(kParsedURLString, "http://example.com/root.js")));
  }

  auto url_dep = url_deps.back();
  url_deps.pop_back();
  GetModulator()->ResolveDependentTreeFetch(
      url_dep, ModuleTreeLinkerTestModulator::ResolveResult::kSuccess);
  EXPECT_FALSE(client->WasNotifyFinished());
  url_dep = url_deps.back();
  url_deps.pop_back();
  GetModulator()->ResolveDependentTreeFetch(
      url_dep, ModuleTreeLinkerTestModulator::ResolveResult::kFailure);

  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_FALSE(client->GetModuleScript());

  // Check below doesn't crash.
  url_dep = url_deps.back();
  url_deps.pop_back();
  GetModulator()->ResolveDependentTreeFetch(
      url_dep, ModuleTreeLinkerTestModulator::ResolveResult::kSuccess);
  EXPECT_TRUE(url_deps.IsEmpty());
}

TEST_F(ModuleTreeLinkerTest, FetchDependencyTree) {
  ModuleTreeLinkerRegistry* registry = ModuleTreeLinkerRegistry::Create();

  KURL url(kParsedURLString, "http://example.com/depth1.js");
  ModuleScriptFetchRequest module_request(
      url, String(), kParserInserted, WebURLRequest::kFetchCredentialsModeOmit);
  TestModuleTreeClient* client = new TestModuleTreeClient;
  registry->Fetch(
      module_request,
      AncestorList{KURL(kParsedURLString, "http://example.com/root.js")},
      ModuleGraphLevel::kDependentModuleFetch, GetModulator(), client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(
      url, {"./depth2.js"}, ModuleInstantiationState::kUninstantiated);

  KURL url_dep2(kParsedURLString, "http://example.com/depth2.js");
  auto ancestor_list = GetModulator()->GetAncestorListForTreeFetch(url_dep2);
  EXPECT_EQ(2u, ancestor_list.size());
  EXPECT_TRUE(ancestor_list.Contains(
      KURL(kParsedURLString, "http://example.com/root.js")));
  EXPECT_TRUE(ancestor_list.Contains(
      KURL(kParsedURLString, "http://example.com/depth1.js")));

  GetModulator()->ResolveDependentTreeFetch(
      url_dep2, ModuleTreeLinkerTestModulator::ResolveResult::kSuccess);

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_EQ(client->GetModuleScript()->State(),
            ModuleInstantiationState::kInstantiated);
}

TEST_F(ModuleTreeLinkerTest, FetchDependencyOfCyclicGraph) {
  ModuleTreeLinkerRegistry* registry = ModuleTreeLinkerRegistry::Create();

  KURL url(kParsedURLString, "http://example.com/a.js");
  ModuleScriptFetchRequest module_request(
      url, String(), kParserInserted, WebURLRequest::kFetchCredentialsModeOmit);
  TestModuleTreeClient* client = new TestModuleTreeClient;
  registry->Fetch(
      module_request,
      AncestorList{KURL(kParsedURLString, "http://example.com/a.js")},
      ModuleGraphLevel::kDependentModuleFetch, GetModulator(), client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleTreeLinker should always finish asynchronously.";
  EXPECT_FALSE(client->GetModuleScript());

  GetModulator()->ResolveSingleModuleScriptFetch(
      url, {"./a.js"}, ModuleInstantiationState::kUninstantiated);

  auto ancestor_list = GetModulator()->GetAncestorListForTreeFetch(url);
  EXPECT_EQ(0u, ancestor_list.size());

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_EQ(client->GetModuleScript()->State(),
            ModuleInstantiationState::kInstantiated);
}

}  // namespace blink
