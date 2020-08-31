// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/api/extension_action/test_extension_action_api_observer.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/declarative_net_request/action_tracker.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/declarative_net_request_api.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/blocked_action_type.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/file_util.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ipc/ipc_message.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {
namespace declarative_net_request {
namespace {

namespace dnr_api = api::declarative_net_request;

using ::testing::UnorderedElementsAre;

constexpr char kDefaultRulesetID[] = "id";

// Returns true if |window.scriptExecuted| is true for the given frame.
bool WasFrameWithScriptLoaded(content::RenderFrameHost* rfh) {
  if (!rfh)
    return false;
  bool script_resource_was_loaded = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      rfh, "domAutomationController.send(!!window.scriptExecuted)",
      &script_resource_was_loaded));
  return script_resource_was_loaded;
}

// Helper to wait for warnings thrown for a given extension. This must be
// constructed before warnings are added.
class WarningServiceObserver : public WarningService::Observer {
 public:
  WarningServiceObserver(WarningService* warning_service,
                         const ExtensionId& extension_id)
      : observer_(this), extension_id_(extension_id) {
    observer_.Add(warning_service);
  }
  WarningServiceObserver(const WarningServiceObserver&) = delete;
  WarningServiceObserver& operator=(const WarningServiceObserver&) = delete;

  // Should only be called once per WarningServiceObserver lifetime.
  void WaitForWarning() { run_loop_.Run(); }

 private:
  // WarningService::Observer override:
  void ExtensionWarningsChanged(
      const ExtensionIdSet& affected_extensions) override {
    if (!base::Contains(affected_extensions, extension_id_))
      return;

    run_loop_.Quit();
  }

  ScopedObserver<WarningService, WarningService::Observer> observer_;
  const ExtensionId extension_id_;
  base::RunLoop run_loop_;
};

// Helper to wait for ruleset load in response to extension load.
class RulesetLoadObserver : public RulesMonitorService::TestObserver {
 public:
  RulesetLoadObserver(RulesMonitorService* service,
                      const ExtensionId& extension_id)
      : service_(service), extension_id_(extension_id) {
    service_->SetObserverForTest(this);
  }

  ~RulesetLoadObserver() override { service_->SetObserverForTest(nullptr); }

  void Wait() { run_loop_.Run(); }

 private:
  // RulesMonitorService::TestObserver override:
  void OnRulesetLoadComplete(const ExtensionId& extension_id) override {
    if (extension_id_ == extension_id)
      run_loop_.Quit();
  }

  RulesMonitorService* const service_;
  const ExtensionId extension_id_;
  base::RunLoop run_loop_;
};

class DeclarativeNetRequestBrowserTest
    : public ExtensionBrowserTest,
      public ::testing::WithParamInterface<ExtensionLoadType> {
 public:
  DeclarativeNetRequestBrowserTest() {
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
  }

  // ExtensionBrowserTest overrides:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    base::FilePath test_root_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
    test_root_path = test_root_path.AppendASCII("extensions")
                         .AppendASCII("declarative_net_request");
    embedded_test_server()->ServeFilesFromDirectory(test_root_path);

    embedded_test_server()->RegisterRequestMonitor(
        base::BindRepeating(&DeclarativeNetRequestBrowserTest::MonitorRequest,
                            base::Unretained(this)));

    content::SetupCrossSiteRedirector(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());

    // Map all hosts to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ruleset_manager_observer_ =
        std::make_unique<RulesetManagerObserver>(ruleset_manager());
  }

  void TearDownOnMainThread() override {
    // Ensure |ruleset_manager_observer_| gets destructed on the UI thread.
    ruleset_manager_observer_.reset();

    ExtensionBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Returns the number of extensions with active rulesets.
  size_t extensions_with_rulesets_count() {
    return ruleset_manager()->GetMatcherCountForTest();
  }

  RulesetManagerObserver* ruleset_manager_observer() {
    return ruleset_manager_observer_.get();
  }

  // Waits till the number of extensions with active rulesets is |count|.
  void WaitForExtensionsWithRulesetsCount(size_t count) {
    ruleset_manager_observer()->WaitForExtensionsWithRulesetsCount(count);
  }

  content::WebContents* web_contents(Browser* browser) const {
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* web_contents() const { return web_contents(browser()); }

  content::RenderFrameHost* GetMainFrame(Browser* browser) const {
    return web_contents(browser)->GetMainFrame();
  }

  content::RenderFrameHost* GetMainFrame() const {
    return GetMainFrame(browser());
  }

  content::RenderFrameHost* GetFrameByName(const std::string& name) const {
    return content::FrameMatchingPredicate(
        web_contents(), base::Bind(&content::FrameMatchesName, name));
  }

  content::PageType GetPageType(Browser* browser) const {
    return web_contents(browser)
        ->GetController()
        .GetLastCommittedEntry()
        ->GetPageType();
  }

  RulesMonitorService* rules_monitor_service() {
    return RulesMonitorService::Get(profile());
  }

  RulesetManager* ruleset_manager() {
    return rules_monitor_service()->ruleset_manager();
  }

  const Extension* last_loaded_extension() {
    return extension_registry()->GetExtensionById(last_loaded_extension_id(),
                                                  ExtensionRegistry::ENABLED);
  }

  content::PageType GetPageType() const { return GetPageType(browser()); }

  std::string GetPageBody() const {
    std::string result;
    const char* script =
        "domAutomationController.send(document.body.innerText.trim())";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(web_contents(), script,
                                                       &result));
    return result;
  }

  void set_config_flags(unsigned flags) { flags_ = flags; }

  // Loads an extension with the given |rulesets| in the given |directory|.
  // Generates a fatal failure if the extension failed to load. |hosts|
  // specifies the host permissions the extensions should have. Waits till the
  // ruleset is loaded.
  void LoadExtensionWithRulesets(const std::vector<TestRulesetInfo>& rulesets,
                                 const std::string& directory,
                                 const std::vector<std::string>& hosts) {
    size_t expected_extension_ruleset_count_change = rulesets.empty() ? 0 : 1;
    LoadExtensionInternal(
        rulesets, directory, hosts, expected_extension_ruleset_count_change,
        false /* has_dynamic_ruleset */, false /* is_extension_update */);
  }

  // Similar to LoadExtensionWithRulesets above but updates the last loaded
  // extension instead. |expected_extension_ruleset_count_change| corresponds to
  // the expected change in the number of extensions with rulesets after
  // extension update. |has_dynamic_ruleset| should be true if the installed
  // extension has a dynamic ruleset.
  void UpdateLastLoadedExtension(
      const std::vector<TestRulesetInfo>& new_rulesets,
      const std::string& new_directory,
      const std::vector<std::string>& new_hosts,
      int expected_extension_ruleset_count_change,
      bool has_dynamic_ruleset) {
    LoadExtensionInternal(new_rulesets, new_directory, new_hosts,
                          expected_extension_ruleset_count_change,
                          has_dynamic_ruleset, true /* is_extension_update */);
  }

  // Specialization of LoadExtensionWithRulesets above for an extension with a
  // single static ruleset.
  void LoadExtensionWithRules(const std::vector<TestRule>& rules,
                              const std::string& directory,
                              const std::vector<std::string>& hosts) {
    constexpr char kJSONRulesFilename[] = "rules_file.json";
    LoadExtensionWithRulesets(
        {TestRulesetInfo(kDefaultRulesetID, kJSONRulesFilename,
                         *ToListValue(rules))},
        directory, hosts);
  }

  // Returns a url with |filter| as a substring.
  GURL GetURLForFilter(const std::string& filter) const {
    return embedded_test_server()->GetURL(
        "abc.com", "/pages_with_script/index.html?" + filter);
  }

  void LoadExtensionWithRules(const std::vector<TestRule>& rules) {
    LoadExtensionWithRules(rules, "test_extension", {} /* hosts */);
  }

  // Returns true if the navigation to given |url| is blocked.
  bool IsNavigationBlocked(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
    return !WasFrameWithScriptLoaded(GetMainFrame());
  }

  void VerifyNavigations(const std::vector<GURL>& expected_blocked_urls,
                         const std::vector<GURL>& expected_allowed_urls) {
    for (const GURL& url : expected_blocked_urls)
      EXPECT_TRUE(IsNavigationBlocked(url)) << url;

    for (const GURL& url : expected_allowed_urls)
      EXPECT_FALSE(IsNavigationBlocked(url)) << url;
  }

  TestRule CreateMainFrameBlockRule(const std::string& filter) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = filter;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    return rule;
  }

  void AddDynamicRules(const ExtensionId& extension_id,
                       const std::vector<TestRule>& rules) {
    static constexpr char kScript[] = R"(
      chrome.declarativeNetRequest.updateDynamicRules([], $1, function () {
        window.domAutomationController.send(chrome.runtime.lastError ?
            chrome.runtime.lastError.message : 'success');
      });
    )";

    // Serialize |rules|.
    ListBuilder builder;
    for (const auto& rule : rules)
      builder.Append(rule.ToValue());

    // A cast is necessary from ListValue to Value, else this fails to compile.
    const std::string script = content::JsReplace(
        kScript, static_cast<const base::Value&>(*builder.Build()));
    ASSERT_EQ("success", ExecuteScriptInBackgroundPage(extension_id, script));
  }

  void RemoveDynamicRules(const ExtensionId& extension_id,
                          const std::vector<int> rule_ids) {
    static constexpr char kScript[] = R"(
      chrome.declarativeNetRequest.updateDynamicRules($1, [], function () {
        window.domAutomationController.send(chrome.runtime.lastError ?
            chrome.runtime.lastError.message : 'success');
      });
    )";

    // Serialize |rule_ids|.
    std::unique_ptr<base::Value> rule_ids_value =
        ListBuilder().Append(rule_ids.begin(), rule_ids.end()).Build();

    // A cast is necessary from ListValue to Value, else this fails to compile.
    const std::string script = content::JsReplace(
        kScript, static_cast<const base::Value&>(*rule_ids_value));
    ASSERT_EQ("success", ExecuteScriptInBackgroundPage(extension_id, script));
  }

  void UpdateEnabledRulesets(
      const ExtensionId& extension_id,
      const std::vector<std::string>& ruleset_ids_to_remove,
      const std::vector<std::string>& ruleset_ids_to_add) {
    static constexpr char kScript[] = R"(
      chrome.declarativeNetRequest.updateEnabledRulesets($1, $2, () => {
        window.domAutomationController.send(chrome.runtime.lastError ?
            chrome.runtime.lastError.message : 'success');
      });
    )";

    std::unique_ptr<base::Value> ids_to_remove =
        ListBuilder()
            .Append(ruleset_ids_to_remove.begin(), ruleset_ids_to_remove.end())
            .Build();
    std::unique_ptr<base::Value> ids_to_add =
        ListBuilder()
            .Append(ruleset_ids_to_add.begin(), ruleset_ids_to_add.end())
            .Build();

    // A cast is necessary from ListValue to Value, else this fails to compile.
    const std::string script = content::JsReplace(
        kScript, static_cast<const base::Value&>(*ids_to_remove),
        static_cast<const base::Value&>(*ids_to_add));
    ASSERT_EQ("success", ExecuteScriptInBackgroundPage(extension_id, script));
  }

  void SetActionsAsBadgeText(const ExtensionId& extension_id, bool pref) {
    const char* pref_string = pref ? "true" : "false";
    static constexpr char kSetActionCountAsBadgeTextScript[] = R"(
      chrome.declarativeNetRequest.setActionCountAsBadgeText(%s);
      window.domAutomationController.send("done");
    )";

    ExecuteScriptInBackgroundPage(
        extension_id,
        base::StringPrintf(kSetActionCountAsBadgeTextScript, pref_string));
  }

  // Navigates frame with name |frame_name| to |url|.
  void NavigateFrame(const std::string& frame_name,
                     const GURL& url,
                     bool use_frame_referrer = true) {
    content::TestNavigationObserver navigation_observer(
        web_contents(), 1 /*number_of_navigations*/);

    const char* referrer_policy = use_frame_referrer ? "origin" : "no-referrer";

    ASSERT_TRUE(content::ExecuteScript(
        GetMainFrame(),
        base::StringPrintf(R"(
          document.getElementsByName('%s')[0].referrerPolicy = '%s';
          document.getElementsByName('%s')[0].src = '%s';)",
                           frame_name.c_str(), referrer_policy,
                           frame_name.c_str(), url.spec().c_str())));
    navigation_observer.Wait();
  }

  // Calls getMatchedRules for |extension_id| and optionally, the |tab_id| and
  // returns comma separated pairs of rule_id and tab_id, with each pair
  // delimited by '|'. Matched Rules are sorted in ascending order by ruleId,
  // and ties are resolved by the tabId (in ascending order.)
  // E.g. "<rule_1>,<tab_1>|<rule_2>,<tab_2>|<rule_3>,<tab_3>"
  std::string GetRuleAndTabIdsMatched(const ExtensionId& extension_id,
                                      base::Optional<int> tab_id) {
    const char kGetMatchedRulesScript[] = R"(
      chrome.declarativeNetRequest.getMatchedRules({%s}, (rules) => {
        // |ruleAndTabIds| is a list of `${ruleId},${tabId}`
        var ruleAndTabIds = rules.rulesMatchedInfo.map(rule => {
          return [rule.rule.ruleId, rule.tabId];
        }).sort((a, b) => {
          // Sort ascending by rule ID, and resolve ties by tab ID.
          const idDiff = a.ruleId - b.ruleId;
          return (idDiff != 0) ? idDiff : a.tabId - b.tabId;
        }).map(ruleAndTabId => ruleAndTabId.join());

        // Join the comma separated (ruleId,tabId) pairs with '|'.
        window.domAutomationController.send(ruleAndTabIds.join('|'));
      });
    )";

    std::string tab_id_param =
        tab_id ? base::StringPrintf("tabId: %d", *tab_id) : "";
    return ExecuteScriptInBackgroundPage(
        extension_id,
        base::StringPrintf(kGetMatchedRulesScript, tab_id_param.c_str()),
        browsertest_util::ScriptUserActivation::kDontActivate);
  }

  // Calls getMatchedRules for |extension_id| and optionally, |tab_id| and
  // |timestamp|. Returns the matched rule count for rules more recent than
  // |timestamp| if specified, and are associated with the tab specified by
  // |tab_id| or all tabs if |tab_id| is not specified. |script_user_activation|
  // specifies if the call should be treated as a user gesture. Returns any API
  // error if the call fails.
  std::string GetMatchedRuleCount(
      const ExtensionId& extension_id,
      base::Optional<int> tab_id,
      base::Optional<base::Time> timestamp,
      browsertest_util::ScriptUserActivation script_user_activation =
          browsertest_util::ScriptUserActivation::kDontActivate) {
    const char kGetMatchedRulesScript[] = R"(
      chrome.declarativeNetRequest.getMatchedRules(%s, (rules) => {
        if (chrome.runtime.lastError) {
          window.domAutomationController.send(chrome.runtime.lastError.message);
          return;
        }

        var rule_count = rules.rulesMatchedInfo.length;
        window.domAutomationController.send(rule_count.toString());
      });
    )";

    double timestamp_in_js =
        timestamp.has_value() ? timestamp->ToJsTimeIgnoringNull() : 0;

    std::string param_string =
        tab_id.has_value()
            ? base::StringPrintf("{tabId: %d, minTimeStamp: %f}", *tab_id,
                                 timestamp_in_js)
            : base::StringPrintf("{minTimeStamp: %f}", timestamp_in_js);

    return ExecuteScriptInBackgroundPage(
        extension_id,
        base::StringPrintf(kGetMatchedRulesScript, param_string.c_str()),
        script_user_activation);
  }

  std::set<GURL> GetAndResetRequestsToServer() {
    base::AutoLock lock(requests_to_server_lock_);
    std::set<GURL> results = requests_to_server_;
    requests_to_server_.clear();
    return results;
  }

 private:
  // Handler to monitor the requests which reach the EmbeddedTestServer. This
  // will be run on the EmbeddedTestServer's IO thread.
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    base::AutoLock lock(requests_to_server_lock_);
    requests_to_server_.insert(request.GetURL());
  }

  // Helper to load an extension. |has_dynamic_ruleset| should be true if the
  // extension has a dynamic ruleset on load. If |is_extension_update|, the last
  // loaded extension is updated.
  void LoadExtensionInternal(const std::vector<TestRulesetInfo>& rulesets,
                             const std::string& directory,
                             const std::vector<std::string>& hosts,
                             int expected_extension_ruleset_count_change,
                             bool has_dynamic_ruleset,
                             bool is_extension_update) {
    CHECK(!is_extension_update || GetParam() == ExtensionLoadType::PACKED);

    // The "crx" directory is reserved for use by this test fixture.
    CHECK_NE("crx", directory);

    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    base::HistogramTester tester;

    base::FilePath extension_dir = temp_dir_.GetPath().AppendASCII(directory);
    ASSERT_FALSE(base::PathExists(extension_dir));
    EXPECT_TRUE(base::CreateDirectory(extension_dir));

    WriteManifestAndRulesets(extension_dir, rulesets, hosts, flags_);

    ExtensionTestMessageListener background_page_ready_listener(
        "ready", false /*will_reply*/);
    size_t current_ruleset_count = extensions_with_rulesets_count();

    const Extension* extension = nullptr;
    switch (GetParam()) {
      case ExtensionLoadType::PACKED: {
        base::FilePath crx_dir =
            temp_dir_.GetPath().AppendASCII("crx").AppendASCII(directory);
        base::FilePath crx_path = crx_dir.AppendASCII("temp.crx");

        base::FilePath pem_path;
        if (is_extension_update)
          pem_path = last_pem_path_;
        else
          last_pem_path_ = crx_dir.AppendASCII("temp.pem");

        ASSERT_FALSE(base::PathExists(crx_dir));
        ASSERT_TRUE(base::CreateDirectory(crx_dir));
        ASSERT_EQ(crx_path,
                  PackExtensionWithOptions(extension_dir, crx_path, pem_path,
                                           last_pem_path_ /* pem_out_path */
                                           ));

        if (is_extension_update) {
          std::string extension_id = last_loaded_extension_id();
          extension =
              UpdateExtension(extension_id, crx_path, 0 /* expected_change */);
        } else {
          extension = InstallExtensionWithPermissionsGranted(
              crx_path, 1 /* expected_change */);
        }
        break;
      }
      case ExtensionLoadType::UNPACKED:
        extension = LoadExtension(extension_dir);
        break;
    }

    ASSERT_TRUE(extension);

    WaitForExtensionsWithRulesetsCount(current_ruleset_count +
                                       expected_extension_ruleset_count_change);

    size_t expected_enabled_rulesets_count = has_dynamic_ruleset ? 1 : 0;
    size_t expected_manifest_rules_count = 0;
    size_t expected_manifest_enabled_rules_count = 0;
    for (const TestRulesetInfo& info : rulesets) {
      size_t rules_count = info.rules_value.GetList().size();
      expected_manifest_rules_count += rules_count;

      if (info.enabled) {
        expected_enabled_rulesets_count++;
        expected_manifest_enabled_rules_count += rules_count;
      }
    }

    // The histograms below are not logged for unpacked extensions.
    if (GetParam() == ExtensionLoadType::PACKED) {
      size_t expected_histogram_counts = rulesets.empty() ? 0 : 1;

      tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram,
                              expected_histogram_counts);
      tester.ExpectBucketCount(kManifestRulesCountHistogram,
                               expected_manifest_rules_count /*sample*/,
                               expected_histogram_counts);
      tester.ExpectBucketCount(kManifestEnabledRulesCountHistogram,
                               expected_manifest_enabled_rules_count /*sample*/,
                               expected_histogram_counts);
    }
    tester.ExpectTotalCount(
        "Extensions.DeclarativeNetRequest.CreateVerifiedMatcherTime",
        expected_enabled_rulesets_count);
    tester.ExpectUniqueSample(
        "Extensions.DeclarativeNetRequest.LoadRulesetResult",
        RulesetMatcher::kLoadSuccess /*sample*/,
        expected_enabled_rulesets_count);

    EXPECT_TRUE(AreAllIndexedStaticRulesetsValid(*extension, profile()));

    // Wait for the background page to load if needed.
    if (flags_ & kConfig_HasBackgroundScript) {
      ASSERT_TRUE(background_page_ready_listener.WaitUntilSatisfied());
      ASSERT_EQ(extension->id(),
                background_page_ready_listener.extension_id_for_message());
    }

    // Ensure no load errors were reported.
    EXPECT_TRUE(LoadErrorReporter::GetInstance()->GetErrors()->empty());
  }

  base::ScopedTempDir temp_dir_;

  unsigned flags_ = ConfigFlag::kConfig_None;

  // Requests observed by the EmbeddedTestServer. This is accessed on both the
  // UI and the EmbeddedTestServer's IO thread. Access is protected by
  // |requests_to_server_lock_|.
  std::set<GURL> requests_to_server_;

  base::Lock requests_to_server_lock_;

  std::unique_ptr<RulesetManagerObserver> ruleset_manager_observer_;

  // Path to the PEM file for the last installed packed extension.
  base::FilePath last_pem_path_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestBrowserTest);
};

using DeclarativeNetRequestBrowserTest_Packed =
    DeclarativeNetRequestBrowserTest;
using DeclarativeNetRequestBrowserTest_Unpacked =
    DeclarativeNetRequestBrowserTest;

#if defined(OS_WIN) && !defined(NDEBUG)
// TODO: test times out on win7-debug. http://crbug.com/900447.
#define MAYBE_BlockRequests_UrlFilter DISABLED_BlockRequests_UrlFilter
#else
#define MAYBE_BlockRequests_UrlFilter BlockRequests_UrlFilter
#endif
// Tests the "urlFilter" and "regexFilter" property of a declarative rule
// condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       MAYBE_BlockRequests_UrlFilter) {
  struct {
    std::string filter;
    int id;
    bool is_regex_rule;
  } rules_data[] = {
      {"pages_with_script/*ex", 1, false},
      {"||a.b.com", 2, false},
      {"|http://*.us", 3, false},
      {"pages_with_script/page2.html|", 4, false},
      {"|http://msn*/pages_with_script/page.html|", 5, false},
      {"%20", 6, false},     // Block any urls with space.
      {"%C3%A9", 7, false},  // Percent-encoded non-ascii character é.
      // Internationalized domain "ⱴase.com" in punycode.
      {"|http://xn--ase-7z0b.com", 8, false},
      {R"((http|https)://(\w+\.){1,2}com.*reg$)", 9, true},
      {R"(\d+\.google\.com)", 10, true},
  };

  // Rule |i| is the rule with id |i|.
  struct {
    std::string hostname;
    std::string path;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      {"example.com", "/pages_with_script/index.html", false},  // Rule 1
      {"example.com", "/pages_with_script/page.html", true},
      {"a.b.com", "/pages_with_script/page.html", false},    // Rule 2
      {"c.a.b.com", "/pages_with_script/page.html", false},  // Rule 2
      {"b.com", "/pages_with_script/page.html", true},
      {"example.us", "/pages_with_script/page.html", false},  // Rule 3
      {"example.jp", "/pages_with_script/page.html", true},
      {"example.jp", "/pages_with_script/page2.html", false},  // Rule 4
      {"example.jp", "/pages_with_script/page2.html?q=hello", true},
      {"msn.com", "/pages_with_script/page.html", false},  // Rule 5
      {"msn.com", "/pages_with_script/page.html?q=hello", true},
      {"a.msn.com", "/pages_with_script/page.html", true},
      {"abc.com", "/pages_with_script/page.html?q=hi bye", false},    // Rule 6
      {"abc.com", "/pages_with_script/page.html?q=hi%20bye", false},  // Rule 6
      {"abc.com", "/pages_with_script/page.html?q=hibye", true},
      {"abc.com",
       "/pages_with_script/page.html?q=" + base::WideToUTF8(L"\u00E9"),
       false},  // Rule 7
      {base::WideToUTF8(L"\x2c74"
                        L"ase.com"),
       "/pages_with_script/page.html", false},                  // Rule 8
      {"abc.com", "/pages_with_script/page2.html?reg", false},  // Rule 9
      {"abc.com", "/pages_with_script/page2.html?reg1", true},
      {"w1.w2.com", "/pages_with_script/page2.html?reg", false},  // Rule 9
      {"w1.w2.w3.com", "/pages_with_script/page2.html?reg", true},
      {"24.google.com", "/pages_with_script/page.html", false},  // Rule 10
      {"xyz.google.com", "/pages_with_script/page.html", true},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter.reset();

    if (rule_data.is_regex_rule)
      rule.condition->regex_filter = rule_data.filter;
    else
      rule.condition->url_filter = rule_data.filter;

    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.id = rule_data.id;
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Verify that the extension correctly intercepts network requests.
  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));

    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests the matching behavior of the separator ('^') character.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_Separator) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter =
      std::string("pages_with_script/page2.html?q=bye^");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // '^' (Separator character) matches anything except a letter, a digit or
  // one of the following: _ - . %.
  struct {
    std::string url_path;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      {"/pages_with_script/page2.html?q=bye&x=1", false},
      {"/pages_with_script/page2.html?q=bye#x=1", false},
      {"/pages_with_script/page2.html?q=bye:", false},
      {"/pages_with_script/page2.html?q=bye3", true},
      {"/pages_with_script/page2.html?q=byea", true},
      {"/pages_with_script/page2.html?q=bye_", true},
      {"/pages_with_script/page2.html?q=bye-", true},
      {"/pages_with_script/page2.html?q=bye%", true},
      {"/pages_with_script/page2.html?q=bye.", true},
  };

  for (const auto& test_case : test_cases) {
    GURL url = embedded_test_server()->GetURL("google.com", test_case.url_path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));

    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_SeparatorMatchesEndOfURL) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("page2.html^");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  GURL url = embedded_test_server()->GetURL("google.com",
                                            "/pages_with_script/page2.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());
}

// Tests case sensitive url filters.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_IsUrlFilterCaseSensitive) {
  struct {
    std::string url_filter;
    size_t id;
    bool is_url_filter_case_sensitive;
  } rules_data[] = {{"pages_with_script/index.html?q=hello", 1, false},
                    {"pages_with_script/page.html?q=hello", 2, true}};

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.condition->is_url_filter_case_sensitive =
        rule_data.is_url_filter_case_sensitive;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Rule |i| is the rule with id |i|.
  struct {
    std::string hostname;
    std::string path;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      {"example.com", "/pages_with_script/index.html?q=hello",
       false},  // Rule 1
      {"example.com", "/pages_with_script/index.html?q=HELLO",
       false},                                                         // Rule 1
      {"example.com", "/pages_with_script/page.html?q=hello", false},  // Rule 2
      {"example.com", "/pages_with_script/page.html?q=HELLO", true},
  };

  for (const auto& test_case : test_cases) {
    GURL url =
        embedded_test_server()->GetURL(test_case.hostname, test_case.path);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));

    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests the "domains" and "excludedDomains" property of a declarative rule
// condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_Domains) {
  struct {
    std::string url_filter;
    size_t id;
    std::vector<std::string> domains;
    std::vector<std::string> excluded_domains;
  } rules_data[] = {{"child_frame.html?frame=1",
                     1,
                     {"x.com", "xn--36c-tfa.com" /* punycode for 36°c.com */},
                     {"a.x.com"}},
                    {"child_frame.html?frame=2", 2, {}, {"a.y.com"}}};

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;

    // An empty list is not allowed for the "domains" property.
    if (!rule_data.domains.empty())
      rule.condition->domains = rule_data.domains;

    rule.condition->excluded_domains = rule_data.excluded_domains;
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Rule |i| is the rule with id |i|.
  struct {
    std::string main_frame_hostname;
    bool expect_frame_1_loaded;
    bool expect_frame_2_loaded;
  } test_cases[] = {
      {"x.com", false /* Rule 1 */, false /* Rule 2 */},
      {base::WideToUTF8(L"36\x00b0"
                        L"c.com" /* 36°c.com */),
       false /*Rule 1*/, false /*Rule 2*/},
      {"b.x.com", false /* Rule 1 */, false /* Rule 2 */},
      {"a.x.com", true, false /* Rule 2 */},
      {"b.a.x.com", true, false /* Rule 2 */},
      {"y.com", true, false /* Rule 2*/},
      {"a.y.com", true, true},
      {"b.a.y.com", true, true},
      {"google.com", true, false /* Rule 2 */},
  };

  for (const auto& test_case : test_cases) {
    GURL url = embedded_test_server()->GetURL(test_case.main_frame_hostname,
                                              "/page_with_two_frames.html");
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    content::RenderFrameHost* main_frame = GetMainFrame();
    EXPECT_TRUE(WasFrameWithScriptLoaded(main_frame));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

    content::RenderFrameHost* child_1 = content::ChildFrameAt(main_frame, 0);
    content::RenderFrameHost* child_2 = content::ChildFrameAt(main_frame, 1);

    EXPECT_EQ(test_case.expect_frame_1_loaded,
              WasFrameWithScriptLoaded(child_1));
    EXPECT_EQ(test_case.expect_frame_2_loaded,
              WasFrameWithScriptLoaded(child_2));
  }
}

// Tests the "domainType" property of a declarative rule condition.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_DomainType) {
  struct {
    std::string url_filter;
    size_t id;
    base::Optional<std::string> domain_type;
  } rules_data[] = {
      {"child_frame.html?case=1", 1, std::string("firstParty")},
      {"child_frame.html?case=2", 2, std::string("thirdParty")},
      {"child_frame.html?case=3", 3, base::nullopt},
  };

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.condition->domain_type = rule_data.domain_type;
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  GURL url =
      embedded_test_server()->GetURL("example.com", "/domain_type_test.html");
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  ASSERT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

  // The loaded page will consist of four pairs of iframes named
  // first_party_[1..4] and third_party_[1..4], with url path
  // child_frame.html?case=[1..4] respectively.
  struct {
    std::string child_frame_name;
    bool expect_frame_loaded;
  } cases[] = {
      // Url matched by rule 1. Only the first party frame is blocked.
      {"first_party_1", false},
      {"third_party_1", true},
      // Url matched by rule 2. Only the third party frame is blocked.
      {"first_party_2", true},
      {"third_party_2", false},
      // Url matched by rule 3. Both the first and third party frames are
      // blocked.
      {"first_party_3", false},
      {"third_party_3", false},
      // No matching rule.
      {"first_party_4", true},
      {"third_party_4", true},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf("Testing child frame named %s",
                                    test_case.child_frame_name.c_str()));

    content::RenderFrameHost* child =
        GetFrameByName(test_case.child_frame_name);
    EXPECT_TRUE(child);
    EXPECT_EQ(test_case.expect_frame_loaded, WasFrameWithScriptLoaded(child));
  }
}

// Tests allowing rules for blocks.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, AllowBlock) {
  const int kNumRequests = 6;

  TestRule rule = CreateGenericRule();
  int id = kMinValidID;

  // Block all main-frame requests ending with numbers 1 to |kNumRequests|.
  std::vector<TestRule> rules;
  for (int i = 1; i <= kNumRequests; ++i) {
    rule.id = id++;
    rule.condition->url_filter = base::StringPrintf("num=%d|", i);
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.priority = 1;
    rules.push_back(rule);
  }

  // Allow all main-frame requests ending with even numbers from 1 to
  // |kNumRequests|.
  for (int i = 2; i <= kNumRequests; i += 2) {
    rule.id = id++;
    rule.condition->url_filter = base::StringPrintf("num=%d|", i);
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = std::string("allow");
    rule.priority = 2;
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  for (int i = 1; i <= kNumRequests; ++i) {
    GURL url = embedded_test_server()->GetURL(
        "example.com",
        base::StringPrintf("/pages_with_script/page.html?num=%d", i));
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);

    // All requests ending with odd numbers should be blocked.
    const bool page_should_load = (i % 2 == 0);
    EXPECT_EQ(page_should_load, WasFrameWithScriptLoaded(GetMainFrame()));
    content::PageType expected_page_type =
        page_should_load ? content::PAGE_TYPE_NORMAL : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests allowing rules for redirects.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, AllowRedirect) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  const GURL static_redirect_url = embedded_test_server()->GetURL(
      "example.com", base::StringPrintf("/pages_with_script/page2.html"));

  const GURL dynamic_redirect_url = embedded_test_server()->GetURL(
      "abc.com", base::StringPrintf("/pages_with_script/page.html"));

  // Create 2 static and 2 dynamic rules.
  struct {
    std::string url_filter;
    int id;
    int priority;
    std::string action_type;
    base::Optional<std::string> redirect_url;
  } rules_data[] = {
      {"google.com", 1, 1, "redirect", static_redirect_url.spec()},
      {"num=1|", 2, 3, "allow", base::nullopt},
      {"1|", 3, 4, "redirect", dynamic_redirect_url.spec()},
      {"num=3|", 4, 2, "allow", base::nullopt},
  };

  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.id = rule_data.id;
    rule.priority = rule_data.priority;
    rule.condition->url_filter = rule_data.url_filter;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = rule_data.action_type;
    rule.action->redirect.emplace();
    rule.action->redirect->url = rule_data.redirect_url;
    rules.push_back(rule);
  }

  std::vector<TestRule> static_rules;
  static_rules.push_back(rules[0]);
  static_rules.push_back(rules[1]);

  std::vector<TestRule> dynamic_rules;
  dynamic_rules.push_back(rules[2]);
  dynamic_rules.push_back(rules[3]);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      static_rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  auto get_url = [this](int i) {
    return embedded_test_server()->GetURL(
        "google.com",
        base::StringPrintf("/pages_with_script/page.html?num=%d", i));
  };

  struct {
    GURL initial_url;
    GURL expected_final_url;
  } static_test_cases[] = {
      {get_url(0), static_redirect_url},
      {get_url(1), get_url(1)},
      {get_url(3), static_redirect_url},
  };

  for (const auto& test_case : static_test_cases) {
    GURL url = test_case.initial_url;
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

    GURL final_url = web_contents()->GetLastCommittedURL();
    EXPECT_EQ(test_case.expected_final_url, final_url);
  }

  // Now add dynamic rules. These should share the priority space with static
  // rules.
  const ExtensionId& extension_id = last_loaded_extension_id();
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, dynamic_rules));

  // Test that dynamic and static rules are in the same priority space.
  struct {
    GURL initial_url;
    GURL expected_final_url;
  } dynamic_test_cases[] = {
      {get_url(1), dynamic_redirect_url},
      {get_url(3), get_url(3)},
  };

  for (const auto& test_case : dynamic_test_cases) {
    GURL url = test_case.initial_url;
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

    GURL final_url = web_contents()->GetLastCommittedURL();
    EXPECT_EQ(test_case.expected_final_url, final_url);
  }
}

// Tests that the extension ruleset is active only when the extension is
// enabled.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       Enable_Disable_Reload_Uninstall) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  // Block all main frame requests to "index.html".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("index.html");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  const ExtensionId extension_id = last_loaded_extension_id();

  // Add dynamic rule to block requests to "page.html".
  rule.condition->url_filter = std::string("page.html");
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));

  GURL static_rule_url = embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/index.html");
  GURL dynamic_rule_url = embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page.html");

  auto test_extension_enabled = [&](bool expected_enabled) {
    EXPECT_EQ(expected_enabled,
              ExtensionRegistry::Get(profile())->enabled_extensions().Contains(
                  extension_id));

    // If the extension is enabled, both the |static_rule_url| and
    // |dynamic_rule_url| should be blocked.
    EXPECT_EQ(expected_enabled, IsNavigationBlocked(static_rule_url));
    EXPECT_EQ(expected_enabled, IsNavigationBlocked(dynamic_rule_url));
  };

  {
    SCOPED_TRACE("Testing extension after load");
    EXPECT_EQ(1u, extensions_with_rulesets_count());
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing DisableExtension");
    DisableExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(0);
    test_extension_enabled(false);
  }

  {
    SCOPED_TRACE("Testing EnableExtension");
    EnableExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(1);
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing ReloadExtension");
    // Don't use ExtensionBrowserTest::ReloadExtension since it waits for the
    // extension to be loaded again. But we need to use our custom waiting logic
    // below.
    extension_service()->ReloadExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(0);
    WaitForExtensionsWithRulesetsCount(1);
    test_extension_enabled(true);
  }

  {
    SCOPED_TRACE("Testing UninstallExtension");
    UninstallExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(0);
    test_extension_enabled(false);
  }
}

// Tests that multiple enabled extensions with declarative rulesets having
// blocking rules behave correctly.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_MultipleExtensions) {
  struct {
    std::string url_filter;
    int id;
    bool add_to_first_extension;
    bool add_to_second_extension;
  } rules_data[] = {
      {"block_both.com", 1, true, true},
      {"block_first.com", 2, true, false},
      {"block_second.com", 3, false, true},
  };

  std::vector<TestRule> rules_1, rules_2;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.id = rule_data.id;
    if (rule_data.add_to_first_extension)
      rules_1.push_back(rule);
    if (rule_data.add_to_second_extension)
      rules_2.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules_1, "extension_1", {URLPattern::kAllUrlsPattern}));
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules_2, "extension_2", {URLPattern::kAllUrlsPattern}));

  struct {
    std::string host;
    bool expect_main_frame_loaded;
  } test_cases[] = {
      {"block_both.com", false},
      {"block_first.com", false},
      {"block_second.com", false},
      {"block_none.com", true},
  };

  for (const auto& test_case : test_cases) {
    GURL url = embedded_test_server()->GetURL(test_case.host,
                                              "/pages_with_script/page.html");
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expect_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));
    content::PageType expected_page_type = test_case.expect_main_frame_loaded
                                               ? content::PAGE_TYPE_NORMAL
                                               : content::PAGE_TYPE_ERROR;
    EXPECT_EQ(expected_page_type, GetPageType());
  }
}

// Tests that multiple enabled extensions with declarative rulesets having
// redirect rules behave correctly with preference given to more recently
// installed extensions.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       RedirectRequests_MultipleExtensions) {
  const int kNumExtensions = 4;

  auto redirect_url_for_extension_number = [this](size_t num) {
    return embedded_test_server()
        ->GetURL(std::to_string(num) + ".com", "/pages_with_script/index.html")
        .spec();
  };

  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.condition->url_filter = std::string("example.com");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});

  base::Time last_extension_install_time = base::Time::Min();

  // Add |kNumExtensions| each redirecting example.com to a different redirect
  // url.
  for (size_t i = 1; i <= kNumExtensions; ++i) {
    rule.action->redirect.emplace();
    rule.action->redirect->url = redirect_url_for_extension_number(i);
    ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
        {rule}, std::to_string(i), {URLPattern::kAllUrlsPattern}));

    // Verify that the install time of this extension is greater than the last
    // extension.
    base::Time install_time = ExtensionPrefs::Get(profile())->GetInstallTime(
        last_loaded_extension_id());
    EXPECT_GT(install_time, last_extension_install_time);
    last_extension_install_time = install_time;
  }

  // Load a url on example.com. It should be redirected to the redirect url
  // corresponding to the most recently installed extension.
  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  GURL final_url = web_contents()->GetLastCommittedURL();
  EXPECT_EQ(GURL(redirect_url_for_extension_number(kNumExtensions)), final_url);
}

// Tests a combination of blocking and redirect rules.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, BlockAndRedirect) {
  auto get_url_for_host = [this](std::string hostname) {
    return embedded_test_server()
        ->GetURL(hostname, "/pages_with_script/index.html")
        .spec();
  };

  struct {
    std::string url_filter;
    int id;
    int priority;
    std::string action_type;
    base::Optional<std::string> redirect_url;
  } rules_data[] = {
      {"example.com", 1, 1, "redirect", get_url_for_host("yahoo.com")},
      {"yahoo.com", 2, 1, "redirect", get_url_for_host("google.com")},
      {"abc.com", 3, 1, "redirect", get_url_for_host("def.com")},
      {"def.com", 4, 2, "block", base::nullopt},
      {"def.com", 5, 1, "redirect", get_url_for_host("xyz.com")},
      {"ghi*", 6, 1, "redirect", get_url_for_host("ghijk.com")},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.priority = rule_data.priority;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = rule_data.action_type;
    rule.action->redirect.emplace();
    rule.action->redirect->url = rule_data.redirect_url;
    rules.push_back(rule);
  }
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  struct {
    std::string hostname;
    bool expected_main_frame_loaded;
    base::Optional<GURL> expected_final_url;
    base::Optional<size_t> expected_redirect_chain_length;
  } test_cases[] = {
      // example.com -> yahoo.com -> google.com.
      {"example.com", true, GURL(get_url_for_host("google.com")), 3},
      // yahoo.com -> google.com.
      {"yahoo.com", true, GURL(get_url_for_host("google.com")), 2},
      // abc.com -> def.com (blocked).
      // Note def.com won't be redirected since blocking rules are given
      // priority over redirect rules.
      {"abc.com", false, base::nullopt, base::nullopt},
      // def.com (blocked).
      {"def.com", false, base::nullopt, base::nullopt},
      // ghi.com -> ghijk.com.
      // Though ghijk.com still matches the redirect rule for |ghi*|, it will
      // not redirect to itself.
      {"ghi.com", true, GURL(get_url_for_host("ghijk.com")), 2},
  };

  for (const auto& test_case : test_cases) {
    std::string url = get_url_for_host(test_case.hostname);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.c_str()));

    ui_test_utils::NavigateToURL(browser(), GURL(url));
    EXPECT_EQ(test_case.expected_main_frame_loaded,
              WasFrameWithScriptLoaded(GetMainFrame()));

    if (!test_case.expected_final_url) {
      EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());
    } else {
      EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

      GURL final_url = web_contents()->GetLastCommittedURL();
      EXPECT_EQ(*test_case.expected_final_url, final_url);

      EXPECT_EQ(*test_case.expected_redirect_chain_length,
                web_contents()
                    ->GetController()
                    .GetLastCommittedEntry()
                    ->GetRedirectChain()
                    .size());
    }
  }
}

// Tests that the redirect url within an extension ruleset is chosen based on
// the highest priority matching rule.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, RedirectPriority) {
  const size_t kNumPatternTypes = 7;

  auto hostname_for_number = [](size_t num) {
    return std::to_string(num) + ".com";
  };

  auto redirect_url_for_priority = [this,
                                    hostname_for_number](size_t priority) {
    return embedded_test_server()
        ->GetURL(hostname_for_number(priority + 1),
                 "/pages_with_script/index.html")
        .spec();
  };

  std::vector<TestRule> rules;
  TestRule rule = CreateGenericRule();
  int id = kMinValidID;
  for (size_t i = 1; i <= kNumPatternTypes; i++) {
    // For pattern type |i|, add |i| rules with priority from 1 to |i|, each
    // with a different redirect url.
    std::string pattern = hostname_for_number(i) + "*page.html";

    for (size_t j = 1; j <= i; j++) {
      rule.id = id++;
      rule.priority = j;
      rule.action->type = std::string("redirect");
      rule.action->redirect.emplace();
      rule.action->redirect->url = redirect_url_for_priority(j);
      rule.condition->url_filter = pattern;
      rule.condition->resource_types = std::vector<std::string>({"main_frame"});
      rules.push_back(rule);
    }
  }

  // Shuffle the rules to ensure that the order in which rules are added has no
  // effect on the test.
  base::RandomShuffle(rules.begin(), rules.end());
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  for (size_t i = 0; i <= kNumPatternTypes + 1; ++i) {
    GURL url = embedded_test_server()->GetURL(hostname_for_number(i),
                                              "/pages_with_script/page.html");
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
    GURL final_url = web_contents()->GetLastCommittedURL();

    bool expect_redirection = i >= 1 && i <= kNumPatternTypes;
    if (expect_redirection) {
      // For pattern type |i|, the highest prioirity for any rule was |i|.
      EXPECT_EQ(GURL(redirect_url_for_priority(i)), final_url);
    } else {
      EXPECT_EQ(url, final_url);
    }
  }
}

// Test that upgradeScheme rules will change the scheme of matching requests to
// https.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, UpgradeRules) {
  auto get_url_for_host = [this](std::string hostname, const char* scheme) {
    GURL url = embedded_test_server()->GetURL(hostname,
                                              "/pages_with_script/index.html");

    url::Replacements<char> replacements;
    replacements.SetScheme(scheme, url::Component(0, strlen(scheme)));

    return url.ReplaceComponents(replacements);
  };

  GURL google_url = get_url_for_host("google.com", url::kHttpScheme);
  struct {
    std::string url_filter;
    int id;
    int priority;
    std::string action_type;
    base::Optional<std::string> redirect_url;
  } rules_data[] = {
      {"exa*", 1, 4, "upgradeScheme", base::nullopt},
      {"|http:*yahoo", 2, 100, "redirect", "http://other.com"},
      // Since the test server can only display http requests, redirect all
      // https requests to google.com in the end.
      // TODO(crbug.com/985104): Add a https test server to display https pages
      // so this redirect rule can be removed.
      {"|https*", 3, 6, "redirect", google_url.spec()},
      {"exact.com", 4, 5, "block", base::nullopt},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.priority = rule_data.priority;
    rule.condition->resource_types = std::vector<std::string>({"main_frame"});
    rule.action->type = rule_data.action_type;
    rule.action->redirect.emplace();
    rule.action->redirect->url = rule_data.redirect_url;
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  // Now load an extension with another ruleset, except this extension has no
  // host permissions.
  TestRule upgrade_rule = CreateGenericRule();
  upgrade_rule.condition->url_filter = "yahoo";
  upgrade_rule.id = kMinValidID;
  upgrade_rule.priority = kMinValidPriority;
  upgrade_rule.condition->resource_types =
      std::vector<std::string>({"main_frame"});
  upgrade_rule.action->type = "upgradeScheme";

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      std::vector<TestRule>({upgrade_rule}), "test_extension_2", {}));

  struct {
    std::string hostname;
    const char* scheme;
    // |expected_final_url| is null if the request is expected to be blocked.
    base::Optional<GURL> expected_final_url;
  } test_cases[] = {
      {"exact.com", url::kHttpScheme, base::nullopt},
      // http://example.com -> https://example.com/ -> http://google.com
      {"example.com", url::kHttpScheme, google_url},
      // test_extension_2 should upgrade the scheme for http://yahoo.com
      // despite having no host permissions. Note that this request is not
      // matched with test_extension_1's ruleset as test_extension_2 is
      // installed more recently.
      // http://yahoo.com -> https://yahoo.com/ -> http://google.com
      {"yahoo.com", url::kHttpScheme, google_url},
  };

  for (const auto& test_case : test_cases) {
    GURL url = get_url_for_host(test_case.hostname, test_case.scheme);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);

    if (!test_case.expected_final_url) {
      EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());
    } else {
      EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

      const GURL& final_url = web_contents()->GetLastCommittedURL();
      EXPECT_EQ(*test_case.expected_final_url, final_url);
    }
  }
}

// Tests that only extensions enabled in incognito mode affect network requests
// from an incognito context.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       BlockRequests_Incognito) {
  // Block all main-frame requests to example.com.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  ExtensionId extension_id = last_loaded_extension_id();
  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/page.html");
  Browser* incognito_browser = CreateIncognitoBrowser();

  auto test_enabled_in_incognito = [&](bool expected_enabled_in_incognito) {
    EXPECT_EQ(expected_enabled_in_incognito,
              util::IsIncognitoEnabled(extension_id, profile()));

    // The url should be blocked in normal context.
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
    EXPECT_EQ(content::PAGE_TYPE_ERROR, GetPageType());

    // In incognito context, the url should be blocked if the extension is
    // enabled in incognito mode.
    ui_test_utils::NavigateToURL(incognito_browser, url);
    EXPECT_EQ(!expected_enabled_in_incognito,
              WasFrameWithScriptLoaded(GetMainFrame(incognito_browser)));
    content::PageType expected_page_type = expected_enabled_in_incognito
                                               ? content::PAGE_TYPE_ERROR
                                               : content::PAGE_TYPE_NORMAL;
    EXPECT_EQ(expected_page_type, GetPageType(incognito_browser));
  };

  {
    // By default, the extension will be disabled in incognito.
    SCOPED_TRACE("Testing extension after load");
    test_enabled_in_incognito(false);
  }

  {
    // Enable the extension in incognito mode.
    SCOPED_TRACE("Testing extension after enabling it in incognito");
    util::SetIsIncognitoEnabled(extension_id, profile(), true /*enabled*/);
    // Toggling the incognito mode reloads the extension. Wait for it to reload.
    WaitForExtensionsWithRulesetsCount(0);
    WaitForExtensionsWithRulesetsCount(1);
    test_enabled_in_incognito(true);
  }

  {
    // Disable the extension in incognito mode.
    SCOPED_TRACE("Testing extension after disabling it in incognito");
    util::SetIsIncognitoEnabled(extension_id, profile(), false /*enabled*/);
    // Toggling the incognito mode reloads the extension. Wait for it to reload.
    WaitForExtensionsWithRulesetsCount(0);
    WaitForExtensionsWithRulesetsCount(1);
    test_enabled_in_incognito(false);
  }
}

// Ensure extensions can't intercept chrome:// urls, even after explicitly
// requesting access to <all_urls>.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, ChromeURLS) {
  // Have the extension block all chrome:// urls.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter =
      std::string(content::kChromeUIScheme) + url::kStandardSchemeSeparator;
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  std::vector<const char*> test_urls = {
      chrome::kChromeUIFlagsURL, chrome::kChromeUIAboutURL,
      chrome::kChromeUIExtensionsURL, chrome::kChromeUIVersionURL};

  for (const char* url : test_urls) {
    ui_test_utils::NavigateToURL(browser(), GURL(url));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  }
}

// Test that a packed extension with a DNR ruleset behaves correctly after
// browser restart.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       PRE_BrowserRestart) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetParam());

  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  // Block all main frame requests to "index.html".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("index.html");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));
  const ExtensionId extension_id = last_loaded_extension_id();

  // Add dynamic rule to block main-frame requests to "page.html".
  rule.condition->url_filter = std::string("page.html");
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));

  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/index.html")));
  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page.html")));
  EXPECT_FALSE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page2.html")));
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       BrowserRestart) {
  // This is not tested for unpacked extensions since the unpacked extension
  // directory won't be persisted across browser restarts.
  ASSERT_EQ(ExtensionLoadType::PACKED, GetParam());

  // Ensure that the DNR extension enabled in previous browser session still
  // correctly blocks network requests.
  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/index.html")));
  EXPECT_TRUE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page.html")));
  EXPECT_FALSE(IsNavigationBlocked(embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page2.html")));
}

// Tests than an extension can omit the "declarative_net_request" manifest key
// but can still use dynamic rules.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, ZeroRulesets) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_OmitDeclarativeNetRequestKey);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRulesets(
      {} /* rulesets */, "extension_directory", {} /* hosts */));
  const ExtensionId extension_id = last_loaded_extension_id();

  const GURL url = embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/page.html");
  EXPECT_FALSE(IsNavigationBlocked(url));

  // Add dynamic rule to block main-frame requests to "page.html".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("page.html");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  EXPECT_EQ(0u, extensions_with_rulesets_count());
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));
  WaitForExtensionsWithRulesetsCount(1);

  EXPECT_TRUE(IsNavigationBlocked(url));

  DisableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(0);
  EXPECT_FALSE(IsNavigationBlocked(url));

  EnableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(1);
  EXPECT_TRUE(IsNavigationBlocked(url));
}

// Test an extension with multiple static rulesets.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, MultipleRulesets) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  const int kNumStaticRulesets = 4;
  const char* kStaticFilterPrefix = "static";
  std::vector<GURL> expected_blocked_urls;
  std::vector<TestRulesetInfo> rulesets;
  std::vector<GURL> expected_allowed_urls;
  for (int i = 0; i < kNumStaticRulesets; ++i) {
    std::vector<TestRule> rules;
    std::string id = kStaticFilterPrefix + base::NumberToString(i);
    rules.push_back(CreateMainFrameBlockRule(id));

    // Enable even indexed rulesets by default.
    bool enabled = i % 2 == 0;
    if (enabled)
      expected_blocked_urls.push_back(GetURLForFilter(id));
    else
      expected_allowed_urls.push_back(GetURLForFilter(id));

    rulesets.emplace_back(id, *ToListValue(rules), enabled);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRulesets(
      rulesets, "extension_directory", {} /* hosts */));
  const ExtensionId extension_id = last_loaded_extension_id();

  // Also add a dynamic rule blocking pages with string "dynamic".
  const char* kDynamicFilter = "dynamic";
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(
      extension_id, {CreateMainFrameBlockRule(kDynamicFilter)}));
  expected_blocked_urls.push_back(GetURLForFilter(kDynamicFilter));

  expected_allowed_urls.push_back(GetURLForFilter("no_such_rule"));

  VerifyNavigations(expected_blocked_urls, expected_allowed_urls);
}

// Ensure that Blink's in-memory cache is cleared on adding/removing rulesets.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, RendererCacheCleared) {
  // Observe requests to RulesetManager to monitor requests to script.js.
  GURL observed_url =
      embedded_test_server()->GetURL("example.com", "/cached/script.js");

  GURL url = embedded_test_server()->GetURL(
      "example.com", "/cached/page_with_cacheable_script.html");

  // With no extension loaded, the request to the script should succeed.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

  // NOTE: RulesetMatcher will not see network requests if no rulesets are
  // active.
  bool expect_request_seen =
      base::FeatureList::IsEnabled(
          extensions_features::kForceWebRequestProxyForTest);
  EXPECT_EQ(expect_request_seen,
            base::Contains(ruleset_manager_observer()->GetAndResetRequestSeen(),
                           observed_url));

  // Another request to |url| should not cause a network request for
  // script.js since it will be served by the renderer's in-memory
  // cache.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_FALSE(base::Contains(
      ruleset_manager_observer()->GetAndResetRequestSeen(), observed_url));

  // Now block requests to script.js.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("script.js");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // Adding an extension ruleset should have cleared the renderer's in-memory
  // cache. Hence the browser process will observe the request to
  // script.js and block it.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_TRUE(base::Contains(
      ruleset_manager_observer()->GetAndResetRequestSeen(), observed_url));

  // Disable the extension.
  DisableExtension(last_loaded_extension_id());
  WaitForExtensionsWithRulesetsCount(0);

  // Disabling the extension should cause the request to succeed again. The
  // request for the script will again be observed by the browser since it's not
  // in the renderer's in-memory cache.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_EQ(expect_request_seen,
            base::Contains(ruleset_manager_observer()->GetAndResetRequestSeen(),
                           observed_url));
}

// Tests that proxy requests aren't intercepted. See https://crbug.com/794674.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       PacRequestsBypassRules) {
  // Load the extension.
  std::vector<TestRule> rules;
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*pac");
  rule.id = 1;
  rules.push_back(rule);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Configure a PAC script. Need to do this after the extension is loaded, so
  // that the PAC isn't already loaded by the time the extension starts
  // affecting requests.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->Set(proxy_config::prefs::kProxy,
                    ProxyConfigDictionary::CreatePacScript(
                        embedded_test_server()->GetURL("/self.pac").spec(),
                        true /* pac_mandatory */));
  // Flush the proxy configuration change over the Mojo pipe to avoid any races.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->FlushProxyConfigMonitorForTesting();

  // Verify that the extension can't intercept the network request.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("http://does.not.resolve.test/pages_with_script/page.html"));
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
}

// Ensure that an extension can't intercept requests on the chrome-extension
// scheme.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       InterceptExtensionScheme) {
  // Load two extensions. One blocks all urls, and the other blocks urls with
  // "google.com" as a substring.
  std::vector<TestRule> rules_1;
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  rules_1.push_back(rule);

  std::vector<TestRule> rules_2;
  rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");
  rules_2.push_back(rule);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules_1, "extension_1", {URLPattern::kAllUrlsPattern}));
  const std::string extension_id_1 = last_loaded_extension_id();

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules_2, "extension_2", {URLPattern::kAllUrlsPattern}));
  const std::string extension_id_2 = last_loaded_extension_id();

  auto get_manifest_url = [](const ExtensionId& extension_id) {
    return GURL(base::StringPrintf("%s://%s/manifest.json",
                                   extensions::kExtensionScheme,
                                   extension_id.c_str()));
  };

  // Extension 1 should not be able to block the request to its own
  // manifest.json or that of the Extension 2, even with "<all_urls>" host
  // permissions.
  ui_test_utils::NavigateToURL(browser(), get_manifest_url(extension_id_1));
  GURL final_url = web_contents()->GetLastCommittedURL();
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  ui_test_utils::NavigateToURL(browser(), get_manifest_url(extension_id_2));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
}

// Ensures that any <img> elements blocked by the API are collapsed.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, ImageCollapsed) {
  // Loads a page with an image and returns whether the image was collapsed.
  auto is_image_collapsed = [this]() {
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("google.com", "/image.html"));
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    bool is_image_collapsed = false;
    const std::string script =
        "domAutomationController.send(!!window.imageCollapsed);";
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents(), script,
                                                     &is_image_collapsed));
    return is_image_collapsed;
  };

  // Initially the image shouldn't be collapsed.
  EXPECT_FALSE(is_image_collapsed());

  // Load an extension which blocks all images.
  std::vector<TestRule> rules;
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  rule.condition->resource_types = std::vector<std::string>({"image"});
  rules.push_back(rule);
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(rules));

  // Now the image request should be blocked and the corresponding DOM element
  // should be collapsed.
  EXPECT_TRUE(is_image_collapsed());
}

// Ensures that any <iframe> elements whose document load is blocked by the API,
// are collapsed.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, IFrameCollapsed) {
  // Verifies whether the frame with name |frame_name| is collapsed.
  auto test_frame_collapse = [this](const std::string& frame_name,
                                    bool expect_collapsed) {
    SCOPED_TRACE(base::StringPrintf("Testing frame %s", frame_name.c_str()));
    content::RenderFrameHost* frame = GetFrameByName(frame_name);
    ASSERT_TRUE(frame);

    EXPECT_EQ(!expect_collapsed, WasFrameWithScriptLoaded(frame));

    constexpr char kScript[] = R"(
        var iframe = document.getElementsByName('%s')[0];
        var collapsed = iframe.clientWidth === 0 && iframe.clientHeight === 0;
        domAutomationController.send(collapsed);
    )";
    bool collapsed = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        GetMainFrame(), base::StringPrintf(kScript, frame_name.c_str()),
        &collapsed));
    EXPECT_EQ(expect_collapsed, collapsed);
  };

  const std::string kFrameName1 = "frame1";
  const std::string kFrameName2 = "frame2";
  const GURL page_url = embedded_test_server()->GetURL(
      "google.com", "/page_with_two_frames.html");

  // Load a page with two iframes (|kFrameName1| and |kFrameName2|). Initially
  // both the frames should be loaded successfully.
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  {
    SCOPED_TRACE("No extension loaded");
    test_frame_collapse(kFrameName1, false);
    test_frame_collapse(kFrameName2, false);
  }

  // Now load an extension which blocks all requests with "frame=1" in its url.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("frame=1");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  // Reloading the page should cause |kFrameName1| to be collapsed.
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  {
    SCOPED_TRACE("Extension loaded initial");
    test_frame_collapse(kFrameName1, true);
    test_frame_collapse(kFrameName2, false);
  }

  // Now interchange the "src" of the two frames. This should cause
  // |kFrameName2| to be collapsed and |kFrameName1| to be un-collapsed.
  GURL frame_url_1 = GetFrameByName(kFrameName1)->GetLastCommittedURL();
  GURL frame_url_2 = GetFrameByName(kFrameName2)->GetLastCommittedURL();
  NavigateFrame(kFrameName1, frame_url_2);
  NavigateFrame(kFrameName2, frame_url_1);
  {
    SCOPED_TRACE("Extension loaded src swapped");
    test_frame_collapse(kFrameName1, false);
    test_frame_collapse(kFrameName2, true);
  }
}

// Tests that we correctly reindex a corrupted ruleset. This is only tested for
// packed extensions, since the JSON ruleset is reindexed on each extension
// load for unpacked extensions, so corruption is not an issue.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       CorruptedIndexedRuleset) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  // Load an extension which blocks all main-frame requests to "google.com".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("||google.com");
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  const ExtensionId extension_id = last_loaded_extension_id();

  // Add a dynamic rule to block main-frame requests to "example.com".
  rule.condition->url_filter = std::string("||example.com");
  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(extension_id, {rule}));

  const GURL static_rule_url = embedded_test_server()->GetURL(
      "google.com", "/pages_with_script/index.html");
  const GURL dynamic_rule_url = embedded_test_server()->GetURL(
      "example.com", "/pages_with_script/index.html");
  const GURL unblocked_url = embedded_test_server()->GetURL(
      "yahoo.com", "/pages_with_script/index.html");

  const Extension* extension = last_loaded_extension();
  std::vector<RulesetSource> static_sources =
      RulesetSource::CreateStatic(*extension);
  ASSERT_EQ(1u, static_sources.size());
  RulesetSource dynamic_source =
      RulesetSource::CreateDynamic(profile(), extension->id());

  // Loading the extension should cause some main frame requests to be blocked.
  {
    SCOPED_TRACE("Page load after loading extension");
    EXPECT_TRUE(IsNavigationBlocked(static_rule_url));
    EXPECT_TRUE(IsNavigationBlocked(dynamic_rule_url));
    EXPECT_FALSE(IsNavigationBlocked(unblocked_url));
  }

  // Helper to overwrite an indexed ruleset file with arbitrary data to mimic
  // corruption, while maintaining the correct version header.
  auto corrupt_file_for_checksum_mismatch =
      [](const base::FilePath& indexed_path) {
        base::ScopedAllowBlockingForTesting scoped_allow_blocking;
        std::string corrupted_data = GetVersionHeaderForTesting() + "data";
        ASSERT_TRUE(base::WriteFile(indexed_path, corrupted_data));
      };

  // Helper to reload the extension and ensure it is working.
  auto test_extension_works_after_reload = [&]() {
    EXPECT_EQ(1u, extensions_with_rulesets_count());
    DisableExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(0);

    EnableExtension(extension_id);
    WaitForExtensionsWithRulesetsCount(1);

    EXPECT_TRUE(IsNavigationBlocked(static_rule_url));
    EXPECT_TRUE(IsNavigationBlocked(dynamic_rule_url));
    EXPECT_FALSE(IsNavigationBlocked(unblocked_url));
  };

  const char* kLoadRulesetResultHistogram =
      "Extensions.DeclarativeNetRequest.LoadRulesetResult";
  const char* kReindexHistogram =
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful";

  // Test static ruleset re-indexing.
  {
    SCOPED_TRACE("Static ruleset corruption");
    corrupt_file_for_checksum_mismatch(static_sources[0].indexed_path());

    base::HistogramTester tester;
    test_extension_works_after_reload();

    // Loading the ruleset would have failed initially due to checksum mismatch
    // and later succeeded.
    tester.ExpectBucketCount(kLoadRulesetResultHistogram,
                             RulesetMatcher::LoadRulesetResult::
                                 kLoadErrorChecksumMismatch /* sample */,
                             1 /* count */);
    // Count of 2 because we load both static and dynamic rulesets.
    tester.ExpectBucketCount(
        kLoadRulesetResultHistogram,
        RulesetMatcher::LoadRulesetResult::kLoadSuccess /* sample */,
        2 /* count */);
    // Verify that reindexing of the static ruleset succeeded.
    tester.ExpectBucketCount(kReindexHistogram, true /*sample*/, 1 /*count*/);
  }

  // Test dynamic ruleset re-indexing.
  {
    SCOPED_TRACE("Dynamic ruleset corruption");
    corrupt_file_for_checksum_mismatch(dynamic_source.indexed_path());

    base::HistogramTester tester;
    test_extension_works_after_reload();

    // Loading the ruleset would have failed initially due to checksum mismatch
    // and later succeeded.
    tester.ExpectBucketCount(kLoadRulesetResultHistogram,
                             RulesetMatcher::LoadRulesetResult::
                                 kLoadErrorChecksumMismatch /* sample */,
                             1 /* count */);
    // Count of 2 because we load both static and dynamic rulesets.
    tester.ExpectBucketCount(
        kLoadRulesetResultHistogram,
        RulesetMatcher::LoadRulesetResult::kLoadSuccess /* sample */,
        2 /* count */);
    // Verify that reindexing of the dynamic ruleset succeeded.
    tester.ExpectBucketCount(kReindexHistogram, true /*sample*/, 1 /*count*/);
  }

  // Go crazy and corrupt both static and dynamic rulesets.
  {
    SCOPED_TRACE("Static and dynamic ruleset corruption");
    corrupt_file_for_checksum_mismatch(dynamic_source.indexed_path());
    corrupt_file_for_checksum_mismatch(static_sources[0].indexed_path());

    base::HistogramTester tester;
    test_extension_works_after_reload();

    // Loading the ruleset would have failed initially due to checksum mismatch
    // and later succeeded.
    tester.ExpectBucketCount(kLoadRulesetResultHistogram,
                             RulesetMatcher::LoadRulesetResult::
                                 kLoadErrorChecksumMismatch /* sample */,
                             2 /* count */);
    // Count of 2 because we load both static and dynamic rulesets.
    tester.ExpectBucketCount(
        kLoadRulesetResultHistogram,
        RulesetMatcher::LoadRulesetResult::kLoadSuccess /* sample */,
        2 /* count */);
    // Verify that reindexing of both the rulesets succeeded.
    tester.ExpectBucketCount(kReindexHistogram, true /*sample*/, 2 /*count*/);
  }
}

// Tests that we surface a warning to the user if any of its ruleset fail to
// load.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       WarningOnFailedRulesetLoad) {
  const size_t kNumStaticRulesets = 4;
  const char* kStaticFilterPrefix = "static";
  std::vector<TestRulesetInfo> rulesets;
  std::vector<GURL> urls_for_indices;
  for (size_t i = 0; i < kNumStaticRulesets; ++i) {
    std::vector<TestRule> rules;
    std::string id = kStaticFilterPrefix + base::NumberToString(i);
    rules.push_back(CreateMainFrameBlockRule(id));

    urls_for_indices.push_back(GetURLForFilter(id));

    rulesets.emplace_back(id, *ToListValue(rules));
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRulesets(
      rulesets, "extension_directory", {} /* hosts */));

  const ExtensionId extension_id = last_loaded_extension_id();
  EXPECT_TRUE(ruleset_manager()->GetMatcherForExtension(extension_id));

  const Extension* extension = last_loaded_extension();
  ASSERT_TRUE(extension);

  std::vector<RulesetSource> sources = RulesetSource::CreateStatic(*extension);
  ASSERT_EQ(kNumStaticRulesets, sources.size());

  // Mimic extension prefs corruption by overwriting the indexed ruleset
  // checksum for the first, third and fourth rulesets.
  const int kInvalidRulesetChecksum = -1;
  std::vector<int> corrupted_ruleset_indices = {0, 2, 3};
  std::vector<int> non_corrupted_ruleset_indices = {1};

  for (int index : corrupted_ruleset_indices) {
    ExtensionPrefs::Get(profile())->SetDNRStaticRulesetChecksum(
        extension_id, sources[index].id(), kInvalidRulesetChecksum);
  }

  DisableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(0);
  EXPECT_FALSE(ruleset_manager()->GetMatcherForExtension(extension_id));

  // Reindexing and loading corrupted rulesets should fail now. This should
  // cause a warning.
  base::HistogramTester tester;
  WarningService* warning_service = WarningService::Get(profile());
  WarningServiceObserver warning_observer(warning_service, extension_id);
  EnableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(1);
  EXPECT_TRUE(ruleset_manager()->GetMatcherForExtension(extension_id));

  // Wait till we surface a warning.
  warning_observer.WaitForWarning();
  EXPECT_THAT(warning_service->GetWarningTypesAffectingExtension(extension_id),
              ::testing::ElementsAre(Warning::kRulesetFailedToLoad));

  // Verify that loading the corrupted rulesets failed due to checksum mismatch.
  // The non-corrupted rulesets should load fine.
  tester.ExpectTotalCount("Extensions.DeclarativeNetRequest.LoadRulesetResult",
                          rulesets.size());
  EXPECT_EQ(corrupted_ruleset_indices.size(),
            static_cast<size_t>(tester.GetBucketCount(
                "Extensions.DeclarativeNetRequest.LoadRulesetResult",
                RulesetMatcher::LoadRulesetResult::
                    kLoadErrorChecksumMismatch /*sample*/)));
  EXPECT_EQ(non_corrupted_ruleset_indices.size(),
            static_cast<size_t>(tester.GetBucketCount(
                "Extensions.DeclarativeNetRequest.LoadRulesetResult",
                RulesetMatcher::LoadRulesetResult::kLoadSuccess /*sample*/)));

  // Verify that re-indexing the corrupted rulesets failed.
  tester.ExpectUniqueSample(
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
      false /*sample*/, corrupted_ruleset_indices.size() /*count*/);

  // Finally verify that the non-corrupted rulesets still work fine, but others
  // don't.
  std::vector<GURL> expected_blocked_urls;
  for (int index : non_corrupted_ruleset_indices)
    expected_blocked_urls.push_back(urls_for_indices[index]);

  std::vector<GURL> expected_allowed_urls;
  for (int index : corrupted_ruleset_indices)
    expected_allowed_urls.push_back(urls_for_indices[index]);

  VerifyNavigations(expected_blocked_urls, expected_allowed_urls);
}

// Tests that we reindex the extension rulesets in case its ruleset format
// version is not the same as one used by Chrome.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       ReindexOnRulesetVersionMismatch) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");

  const int kNumStaticRulesets = 4;
  std::vector<TestRulesetInfo> rulesets;
  for (int i = 0; i < kNumStaticRulesets; ++i)
    rulesets.emplace_back(base::NumberToString(i), *ToListValue({rule}));

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRulesets(
      rulesets, "extension_directory", {} /* hosts */));

  const ExtensionId extension_id = last_loaded_extension_id();
  EXPECT_TRUE(ruleset_manager()->GetMatcherForExtension(extension_id));

  // Add a dynamic rule.
  AddDynamicRules(extension_id, {rule});

  DisableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(0);
  EXPECT_FALSE(ruleset_manager()->GetMatcherForExtension(extension_id));

  // Now change the current indexed ruleset format version. This should cause a
  // version mismatch when the extension is loaded again, but re-indexing should
  // still succeed.
  ScopedIncrementRulesetVersion scoped_version_change =
      CreateScopedIncrementRulesetVersionForTesting();

  // Also override the checksum value for the indexed ruleset to simulate a
  // flatbuffer schema change. This will ensure that the checksum of the
  // re-indexed file differs from the current checksum.
  const int kTestChecksum = 100;
  OverrideGetChecksumForTest(kTestChecksum);

  base::HistogramTester tester;
  EnableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(1);
  EXPECT_TRUE(ruleset_manager()->GetMatcherForExtension(extension_id));

  // We add 1 to include the dynamic ruleset.
  const int kNumRulesets = kNumStaticRulesets + 1;

  // Verify that loading the static and dynamic rulesets would have failed
  // initially due to version header mismatch and later succeeded.
  EXPECT_EQ(kNumRulesets,
            tester.GetBucketCount(
                "Extensions.DeclarativeNetRequest.LoadRulesetResult",
                RulesetMatcher::LoadRulesetResult::
                    kLoadErrorVersionMismatch /*sample*/));
  EXPECT_EQ(kNumRulesets,
            tester.GetBucketCount(
                "Extensions.DeclarativeNetRequest.LoadRulesetResult",
                RulesetMatcher::LoadRulesetResult::kLoadSuccess /*sample*/));

  // Verify that reindexing succeeded.
  tester.ExpectUniqueSample(
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
      true /*sample*/, kNumRulesets /*count*/);

  // Ensure that the new checksum was correctly persisted in prefs.
  const ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  const Extension* extension = last_loaded_extension();
  std::vector<RulesetSource> static_sources =
      RulesetSource::CreateStatic(*extension);
  ASSERT_EQ(static_cast<size_t>(kNumStaticRulesets), static_sources.size());

  int checksum = kTestChecksum + 1;
  for (const RulesetSource& source : static_sources) {
    EXPECT_TRUE(prefs->GetDNRStaticRulesetChecksum(extension_id, source.id(),
                                                   &checksum));
    EXPECT_EQ(kTestChecksum, checksum);

    // Reset checksum for the next test.
    checksum = kTestChecksum + 1;
  }

  EXPECT_TRUE(prefs->GetDNRDynamicRulesetChecksum(extension_id, &checksum));
  EXPECT_EQ(kTestChecksum, checksum);
}

// Tests that static ruleset preferences are deleted on uninstall for an edge
// case where ruleset loading is completed after extension uninstallation.
// Regression test for crbug.com/1067441.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       RulesetPrefsDeletedOnUninstall) {
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({} /* rules */));

  const ExtensionId extension_id = last_loaded_extension_id();
  const Extension* extension = last_loaded_extension();

  std::vector<RulesetSource> static_sources =
      RulesetSource::CreateStatic(*extension);
  ASSERT_EQ(1u, static_sources.size());

  DisableExtension(extension_id);
  WaitForExtensionsWithRulesetsCount(0);

  const ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  int checksum = -1;
  EXPECT_TRUE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, static_sources[0].id(), &checksum));

  // Now change the current indexed ruleset format version. This should cause a
  // version mismatch when the extension is loaded again, but re-indexing should
  // still succeed.
  ScopedIncrementRulesetVersion scoped_version_change =
      CreateScopedIncrementRulesetVersionForTesting();

  // Also override the checksum value for the indexed ruleset to simulate a
  // flatbuffer schema change. This will ensure that the checksum of the
  // re-indexed file differs from the current checksum.
  const int kTestChecksum = checksum + 1;
  OverrideGetChecksumForTest(kTestChecksum);

  base::HistogramTester tester;
  RulesetLoadObserver load_observer(rules_monitor_service(), extension_id);

  // Now enable the extension, causing the asynchronous extension ruleset load
  // which further results in an asynchronous re-indexing task. Immediately
  // uninstall the extension to ensure that the uninstallation precedes
  // completion of ruleset load.
  EnableExtension(extension_id);
  UninstallExtension(extension_id);

  load_observer.Wait();

  // Verify that reindexing succeeded.
  tester.ExpectUniqueSample(
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
      true /*sample*/, 1 /*count*/);

  // Verify that the prefs for the static ruleset were deleted successfully.
  EXPECT_FALSE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, static_sources[0].id(), &checksum));
}

// Tests that redirecting requests using the declarativeNetRequest API works
// with runtime host permissions.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       WithheldPermissions_Redirect) {
  // Load an extension which redirects all script requests made to
  // "b.com/subresources/not_a_valid_script.js", to
  // "b.com/subresources/script.js".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter =
      std::string("b.com/subresources/not_a_valid_script.js");
  rule.condition->resource_types = std::vector<std::string>({"script"});
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url =
      embedded_test_server()->GetURL("b.com", "/subresources/script.js").spec();

  std::vector<std::string> host_permissions = {"*://a.com/", "*://b.com/*"};
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule}, "extension" /* directory */, host_permissions));

  const Extension* extension = last_loaded_extension();
  ASSERT_TRUE(extension);

  auto verify_script_redirected = [this, extension](
                                      const GURL& page_url,
                                      bool expect_script_redirected,
                                      int expected_blocked_actions) {
    ui_test_utils::NavigateToURL(browser(), page_url);

    // The page should have loaded correctly.
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
    EXPECT_EQ(expect_script_redirected,
              WasFrameWithScriptLoaded(GetMainFrame()));

    // The EmbeddedTestServer sees requests after the hostname has been
    // resolved.
    const GURL requested_script_url =
        embedded_test_server()->GetURL("/subresources/not_a_valid_script.js");
    const GURL redirected_script_url =
        embedded_test_server()->GetURL("/subresources/script.js");

    std::set<GURL> seen_requests = GetAndResetRequestsToServer();
    EXPECT_EQ(!expect_script_redirected,
              base::Contains(seen_requests, requested_script_url));
    EXPECT_EQ(expect_script_redirected,
              base::Contains(seen_requests, redirected_script_url));

    ExtensionActionRunner* runner =
        ExtensionActionRunner::GetForWebContents(web_contents());
    ASSERT_TRUE(runner);
    EXPECT_EQ(expected_blocked_actions, runner->GetBlockedActions(extension));
  };

  {
    const GURL page_url = embedded_test_server()->GetURL(
        "example.com", "/cross_site_script.html");
    SCOPED_TRACE(
        base::StringPrintf("Navigating to %s", page_url.spec().c_str()));

    // The extension should not redirect the script request. It has access to
    // the |requested_script_url| but not its initiator |page_url|.
    bool expect_script_redirected = false;
    verify_script_redirected(page_url, expect_script_redirected,
                             BLOCKED_ACTION_NONE);
  }

  {
    const GURL page_url =
        embedded_test_server()->GetURL("a.com", "/cross_site_script.html");
    SCOPED_TRACE(
        base::StringPrintf("Navigating to %s", page_url.spec().c_str()));

    // The extension should redirect the script request. It has access to both
    // the |requested_script_url| and its initiator |page_url|.
    bool expect_script_redirected = true;
    verify_script_redirected(page_url, expect_script_redirected,
                             BLOCKED_ACTION_NONE);
  }

  // Withhold access to all hosts.
  ScriptingPermissionsModifier scripting_modifier(
      profile(), base::WrapRefCounted(extension));
  scripting_modifier.SetWithholdHostPermissions(true);

  {
    const GURL page_url =
        embedded_test_server()->GetURL("a.com", "/cross_site_script.html");
    SCOPED_TRACE(base::StringPrintf("Navigating to %s with all hosts withheld",
                                    page_url.spec().c_str()));

    // The extension should not redirect the script request. It's access to both
    // the |requested_script_url| and its initiator |page_url| is withheld.
    bool expect_script_redirected = false;
    verify_script_redirected(page_url, expect_script_redirected,
                             BLOCKED_ACTION_WEB_REQUEST);
  }

  // Grant access to only "b.com".
  scripting_modifier.GrantHostPermission(GURL("http://b.com"));
  {
    const GURL page_url =
        embedded_test_server()->GetURL("a.com", "/cross_site_script.html");
    SCOPED_TRACE(base::StringPrintf("Navigating to %s with a.com withheld",
                                    page_url.spec().c_str()));

    // The extension should not redirect the script request. It has access to
    // the |requested_script_url|, but its access to the request initiator
    // |page_url| is withheld.
    bool expect_script_redirected = false;
    verify_script_redirected(page_url, expect_script_redirected,
                             BLOCKED_ACTION_WEB_REQUEST);
  }

  // Grant access to only "a.com".
  scripting_modifier.RemoveAllGrantedHostPermissions();
  scripting_modifier.GrantHostPermission(GURL("http://a.com"));
  {
    const GURL page_url =
        embedded_test_server()->GetURL("a.com", "/cross_site_script.html");
    SCOPED_TRACE(base::StringPrintf("Navigating to %s with b.com withheld",
                                    page_url.spec().c_str()));

    // The extension should redirect the script request. It's access to the
    // |requested_script_url| is withheld, but it has access to its initiator
    // |page_url|.
    bool expect_script_redirected = true;
    verify_script_redirected(page_url, expect_script_redirected,
                             BLOCKED_ACTION_NONE);
  }
}

// Ensures that withholding permissions has no effect on blocking requests using
// the declarative net request API.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       WithheldPermissions_Block) {
  // Load an extension with access to <all_urls> which blocks all script
  // requests made on example.com.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  rule.condition->domains = std::vector<std::string>({"example.com"});
  rule.condition->resource_types = std::vector<std::string>({"script"});

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const Extension* extension = last_loaded_extension();
  ASSERT_TRUE(extension);

  EXPECT_TRUE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  // Withhold access to all hosts.
  ScriptingPermissionsModifier scripting_modifier(
      profile(), base::WrapRefCounted(extension));
  scripting_modifier.SetWithholdHostPermissions(true);

  EXPECT_EQ(
      extension->permissions_data()->active_permissions().explicit_hosts(),
      URLPatternSet(
          {URLPattern(URLPattern::SCHEME_CHROMEUI, "chrome://favicon/*")}));

  // Request made by index.html to script.js should be blocked despite the
  // extension having no active host permissions to the request.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/pages_with_script/index.html"));
  EXPECT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));

  // Sanity check that the script.js request is not blocked if does not match a
  // rule.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(
                                   "foo.com", "/pages_with_script/index.html"));
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
}

// Tests the dynamic rule support.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, DynamicRules) {
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  // Add an extension which blocks main-frame requests to "yahoo.com".
  TestRule block_static_rule = CreateGenericRule();
  block_static_rule.condition->resource_types =
      std::vector<std::string>({"main_frame"});
  block_static_rule.condition->url_filter = std::string("||yahoo.com");
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {block_static_rule}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const char* kUrlPath = "/pages_with_script/index.html";
  GURL yahoo_url = embedded_test_server()->GetURL("yahoo.com", kUrlPath);
  GURL google_url = embedded_test_server()->GetURL("google.com", kUrlPath);
  EXPECT_TRUE(IsNavigationBlocked(yahoo_url));
  EXPECT_FALSE(IsNavigationBlocked(google_url));

  // Add dynamic rules to block "google.com" and redirect pages on "example.com"
  // to |dynamic_redirect_url|.
  TestRule block_dynamic_rule = block_static_rule;
  block_dynamic_rule.condition->url_filter = std::string("||google.com");
  block_dynamic_rule.id = kMinValidID;

  GURL dynamic_redirect_url =
      embedded_test_server()->GetURL("dynamic.com", kUrlPath);
  TestRule redirect_rule = CreateGenericRule();
  redirect_rule.condition->url_filter = std::string("||example.com");
  redirect_rule.condition->resource_types =
      std::vector<std::string>({"main_frame"});
  redirect_rule.priority = kMinValidPriority;
  redirect_rule.action->type = std::string("redirect");
  redirect_rule.action->redirect.emplace();
  redirect_rule.action->redirect->url = dynamic_redirect_url.spec();
  redirect_rule.id = kMinValidID + 1;

  ASSERT_NO_FATAL_FAILURE(AddDynamicRules(last_loaded_extension_id(),
                                          {block_dynamic_rule, redirect_rule}));

  EXPECT_TRUE(IsNavigationBlocked(google_url));
  EXPECT_TRUE(IsNavigationBlocked(yahoo_url));

  // Navigate to a page on "example.com". It should be redirected to
  // |dynamic_redirect_url|.
  GURL example_url = embedded_test_server()->GetURL("example.com", kUrlPath);
  ui_test_utils::NavigateToURL(browser(), example_url);
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());
  EXPECT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_EQ(dynamic_redirect_url, web_contents()->GetLastCommittedURL());

  // Now add a dynamic rule to allow requests to yahoo.com.
  TestRule allow_rule = block_static_rule;
  allow_rule.id = kMinValidID + 2;
  allow_rule.action->type = std::string("allow");
  ASSERT_NO_FATAL_FAILURE(
      AddDynamicRules(last_loaded_extension_id(), {allow_rule}));

  // Dynamic ruleset gets more priority over the static ruleset and yahoo.com is
  // not blocked.
  EXPECT_FALSE(IsNavigationBlocked(yahoo_url));

  // Now remove the |block_rule| and |allow_rule|. Rule ids not present will be
  // ignored.
  ASSERT_NO_FATAL_FAILURE(RemoveDynamicRules(
      last_loaded_extension_id(),
      {*block_dynamic_rule.id, *allow_rule.id, kMinValidID + 100}));
  EXPECT_FALSE(IsNavigationBlocked(google_url));
  EXPECT_TRUE(IsNavigationBlocked(yahoo_url));
}

// Tests rules using the Redirect dictionary.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, Redirect) {
  TestRule rule1 = CreateGenericRule();
  rule1.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule1.id = kMinValidID;
  rule1.condition->url_filter = std::string("ex");
  rule1.action->type = std::string("redirect");
  rule1.priority = kMinValidPriority;
  rule1.action->redirect.emplace();
  rule1.action->redirect->url =
      embedded_test_server()
          ->GetURL("google.com", "/pages_with_script/index.html")
          .spec();

  TestRule rule2 = CreateGenericRule();
  rule2.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule2.id = kMinValidID + 1;
  rule2.condition->url_filter = std::string("example.com");
  rule2.action->type = std::string("redirect");
  rule2.priority = kMinValidPriority + 1;
  rule2.action->redirect.emplace();
  rule2.action->redirect->extension_path = "/manifest.json?query#fragment";

  TestRule rule3 = CreateGenericRule();
  rule3.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule3.id = kMinValidID + 2;
  rule3.condition->url_filter = std::string("||example.com");
  rule3.action->type = std::string("redirect");
  rule3.priority = kMinValidPriority + 2;
  rule3.action->redirect.emplace();
  rule3.action->redirect->transform.emplace();
  auto& transform = rule3.action->redirect->transform;
  transform->host = "new.host.com";
  transform->path = "/pages_with_script/page.html";
  transform->query = "?new_query";
  transform->fragment = "#new_fragment";

  TestRule rule4 = CreateGenericRule();
  rule4.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule4.id = kMinValidID + 3;
  rule4.condition->url_filter.reset();
  rule4.condition->regex_filter = R"(^(.+?)://(abc|def)\.exy\.com(.*)$)";
  rule4.action->type = std::string("redirect");
  rule4.priority = kMinValidPriority + 1;
  rule4.action->redirect.emplace();
  rule4.action->redirect->regex_substitution = R"(\1://www.\2.com\3)";

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({rule1, rule2, rule3, rule4}, "test_extension",
                             {URLPattern::kAllUrlsPattern}));

  struct {
    GURL url;
    GURL expected_url;
  } cases[] = {{embedded_test_server()->GetURL("example.com",
                                               "/pages_with_script/index.html"),
                // Because of higher priority, the transform rule is chosen.
                embedded_test_server()->GetURL(
                    "new.host.com",
                    "/pages_with_script/page.html?new_query#new_fragment")},
               // Because of higher priority, the extensionPath rule is chosen.
               {embedded_test_server()->GetURL(
                    "xyz.com", "/pages_with_script/index.html?example.com"),
                GURL("chrome-extension://" + last_loaded_extension_id() +
                     "/manifest.json?query#fragment")},
               {embedded_test_server()->GetURL("ex.com",
                                               "/pages_with_script/index.html"),
                embedded_test_server()->GetURL(
                    "google.com", "/pages_with_script/index.html")},
               // Because of a priority higher than |rule1|, |rule4| is chosen.
               {embedded_test_server()->GetURL("abc.exy.com",
                                               "/pages_with_script/page.html"),
                embedded_test_server()->GetURL("www.abc.com",
                                               "/pages_with_script/page.html")},
               {embedded_test_server()->GetURL("xyz.exy.com",
                                               "/pages_with_script/page.html"),
                embedded_test_server()->GetURL(
                    "google.com", "/pages_with_script/index.html")}};

  for (const auto& test_case : cases) {
    SCOPED_TRACE("Testing " + test_case.url.spec());
    ui_test_utils::NavigateToURL(browser(), test_case.url);
    EXPECT_EQ(test_case.expected_url, web_contents()->GetLastCommittedURL());
  }
}

// Test that the badge text for an extension will update to reflect the number
// of actions taken on requests matching the extension's ruleset.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ActionsMatchedCountAsBadgeText) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it has access to the getMatchedRules API function.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  auto get_url_for_host = [this](std::string hostname) {
    return embedded_test_server()->GetURL(hostname,
                                          "/pages_with_script/index.html");
  };

  // This page simulates a user clicking on a link, so that the next page it
  // navigates to has a Referrer header.
  auto get_url_with_referrer = [this](std::string hostname) {
    return embedded_test_server()->GetURL(hostname, "/simulate_click.html");
  };

  const std::string kFrameName1 = "frame1";
  const GURL page_url = embedded_test_server()->GetURL(
      "norulesmatched.com", "/page_with_two_frames.html");

  struct {
    std::string url_filter;
    int id;
    int priority;
    std::string action_type;
    base::Optional<std::string> redirect_url;
  } rules_data[] = {
      {"abc.com", 1, 1, "block", base::nullopt},
      {"def.com", 2, 1, "redirect", "http://zzz.com"},
      {"abcd.com", 4, 1, "block", base::nullopt},
      {"abcd", 5, 1, "allow", base::nullopt},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.priority = rule_data.priority;
    rule.condition->resource_types =
        std::vector<std::string>({"main_frame", "sub_frame"});
    rule.action->type = rule_data.action_type;
    rule.action->redirect.emplace();
    rule.action->redirect->url = rule_data.redirect_url;
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  const ExtensionId& extension_id = last_loaded_extension_id();
  const Extension* dnr_extension = last_loaded_extension();

  ExtensionPrefs::Get(profile())->SetDNRUseActionCountAsBadgeText(extension_id,
                                                                  true);

  ExtensionAction* action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  struct {
    std::string frame_hostname;
    std::string expected_badge_text;
    bool has_referrer_header;
  } test_cases[] = {
      // zzz.com does not match any rules, so no badge text should be displayed.
      {"zzz.com", "", false},
      // abc.com is blocked by a matching rule and should increment the badge
      // text.
      {"abc.com", "1", false},
      // def.com is redirected by a matching rule and should increment the badge
      // text.
      {"def.com", "2", false},
      // abcd.com matches both a block rule and an allow rule. Since the allow
      // rule overrides the block rule, no action is taken and the badge text
      // stays the same,
      {"abcd.com", "2", false},
  };

  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

  // Verify that the badge text is empty when navigation finishes because no
  // actions have been matched.
  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ("", action->GetDisplayBadgeText(first_tab_id));

  for (const auto& test_case : test_cases) {
    GURL url = test_case.has_referrer_header
                   ? get_url_with_referrer(test_case.frame_hostname)
                   : get_url_for_host(test_case.frame_hostname);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    NavigateFrame(kFrameName1, url, false /* use_frame_referrer */);
    EXPECT_EQ(test_case.expected_badge_text,
              action->GetDisplayBadgeText(first_tab_id));
  }

  std::string first_tab_badge_text = action->GetDisplayBadgeText(first_tab_id);

  const GURL second_tab_url = get_url_for_host("nomatch.com");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), second_tab_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

  int second_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ("", action->GetDisplayBadgeText(second_tab_id));

  // Verify that the badge text for the first tab is unaffected.
  EXPECT_EQ(first_tab_badge_text, action->GetDisplayBadgeText(first_tab_id));

  // Verify that the correct rules are returned via getMatchedRules. Returns "|"
  // separated pairs of <rule_id>,<ruleset_id>, sorted by rule ids, e.g.
  // "ruleId1,rulesetId1|ruleId2,rulesetId2".
  auto get_matched_rules = [this](int tab_id) {
    const char kGetMatchedRulesScript[] = R"(
      chrome.declarativeNetRequest.getMatchedRules({tabId: %d}, (matches) => {
        // Sort by |ruleId|.
        matches.rulesMatchedInfo.sort((a, b) => a.rule.ruleId - b.rule.ruleId);

        var ruleAndRulesetIDs = matches.rulesMatchedInfo.map(
          match => [match.rule.ruleId, match.rule.rulesetId].join());

        window.domAutomationController.send(ruleAndRulesetIDs.join('|'));
      });
    )";

    return ExecuteScriptInBackgroundPage(
        last_loaded_extension_id(),
        base::StringPrintf(kGetMatchedRulesScript, tab_id),
        browsertest_util::ScriptUserActivation::kDontActivate);
  };

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  // Four rules should be matched on the tab with |first_tab_id|:
  //   - the block rule for abc.com (ruleId = 1)
  //   - the redirect rule for def.com (ruleId = 2)
  //   - the allow rule for abcd.com (ruleId = 5)
  EXPECT_EQ(base::StringPrintf("1,%s|2,%s|5,%s", kDefaultRulesetID,
                               kDefaultRulesetID, kDefaultRulesetID),
            get_matched_rules(first_tab_id));

  // No rule should be matched on the tab with |second_tab_id|.
  EXPECT_EQ("", get_matched_rules(second_tab_id));
}

// Ensure web request events are still dispatched even if DNR blocks/redirects
// the request. (Regression test for crbug.com/999744).
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, WebRequestEvents) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = "||example.com";
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rule}, "test_extension", {URLPattern::kAllUrlsPattern}));

  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/index.html");

  // Set up web request listeners listening to request to |url|.
  const char kWebRequestListenerScript[] = R"(
    let filter = {'urls' : ['%s'], 'types' : ['main_frame']};

    let onBeforeRequestSeen = false;
    chrome.webRequest.onBeforeRequest.addListener(() => {
      onBeforeRequestSeen = true;
    }, filter);

    // The request will fail since it will be blocked by DNR.
    chrome.webRequest.onErrorOccurred.addListener(() => {
      if (onBeforeRequestSeen)
        chrome.test.sendMessage('PASS');
    }, filter);

    chrome.test.sendMessage('INSTALLED');
  )";

  ExtensionTestMessageListener pass_listener("PASS", false /* will_reply */);
  ExtensionTestMessageListener installed_listener("INSTALLED",
                                                  false /* will_reply */);
  ExecuteScriptInBackgroundPageNoWait(
      last_loaded_extension_id(),
      base::StringPrintf(kWebRequestListenerScript, url.spec().c_str()));

  // Wait for the web request listeners to be installed before navigating.
  ASSERT_TRUE(installed_listener.WaitUntilSatisfied());

  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_FALSE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_TRUE(pass_listener.WaitUntilSatisfied());
}

// Ensure Declarative Net Request gets priority over the web request API.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, WebRequestPriority) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/pages_with_script/index.html");
  GURL redirect_url = embedded_test_server()->GetURL(
      "redirect.com", "/pages_with_script/index.html");

  TestRule example_com_redirect_rule = CreateGenericRule();
  example_com_redirect_rule.condition->url_filter = "||example.com";
  example_com_redirect_rule.condition->resource_types =
      std::vector<std::string>({"main_frame"});
  example_com_redirect_rule.action->type = std::string("redirect");
  example_com_redirect_rule.action->redirect.emplace();
  example_com_redirect_rule.action->redirect->url = redirect_url.spec();
  example_com_redirect_rule.priority = kMinValidPriority;

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({example_com_redirect_rule}, "test_extension",
                             {URLPattern::kAllUrlsPattern}));

  // Set up a web request listener to block the request to example.com.
  const char kWebRequestBlockScript[] = R"(
    let filter = {'urls' : ['%s'], 'types' : ['main_frame']};
    chrome.webRequest.onBeforeRequest.addListener((details) => {
      chrome.test.sendMessage('SEEN')
    }, filter, ['blocking']);
    chrome.test.sendMessage('INSTALLED');
  )";

  ExtensionTestMessageListener seen_listener("SEEN", false /* will_reply */);
  ExtensionTestMessageListener installed_listener("INSTALLED",
                                                  false /* will_reply */);
  ExecuteScriptInBackgroundPageNoWait(
      last_loaded_extension_id(),
      base::StringPrintf(kWebRequestBlockScript, url.spec().c_str()));

  // Wait for the web request listeners to be installed before navigating.
  ASSERT_TRUE(installed_listener.WaitUntilSatisfied());

  ui_test_utils::NavigateToURL(browser(), url);

  // Ensure the response from the web request listener was ignored and the
  // request was redirected.
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), redirect_url);

  // Ensure onBeforeRequest is seen by the web request extension.
  EXPECT_TRUE(seen_listener.WaitUntilSatisfied());
}

// Test that the extension cannot retrieve the number of actions matched
// from the badge text by calling chrome.browserAction.getBadgeText.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetBadgeTextForActionsMatched) {
  auto query_badge_text_from_ext = [this](const ExtensionId& extension_id,
                                          int tab_id) {
    static constexpr char kBadgeTextQueryScript[] = R"(
        chrome.browserAction.getBadgeText({tabId: %d}, badgeText => {
          window.domAutomationController.send(badgeText);
        });
      )";

    return ExecuteScriptInBackgroundPage(
        extension_id, base::StringPrintf(kBadgeTextQueryScript, tab_id));
  };

  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = "abc.com";
  rule.id = kMinValidID;
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.action->type = "block";

  std::vector<TestRule> rules({rule});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rules}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const ExtensionId& extension_id = last_loaded_extension_id();
  const Extension* dnr_extension = last_loaded_extension();

  ExtensionAction* action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  const std::string default_badge_text = "asdf";
  action->SetBadgeText(ExtensionAction::kDefaultTabId, default_badge_text);

  const GURL page_url = embedded_test_server()->GetURL(
      "abc.com", "/pages_with_script/index.html");
  ui_test_utils::NavigateToURL(browser(), page_url);

  // The preference is initially turned off. Both the visible badge text and the
  // badge text queried by the extension using getBadgeText() should return the
  // default badge text.
  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ(default_badge_text, action->GetDisplayBadgeText(first_tab_id));

  std::string queried_badge_text =
      query_badge_text_from_ext(extension_id, first_tab_id);
  EXPECT_EQ(default_badge_text, queried_badge_text);

  SetActionsAsBadgeText(extension_id, true);
  // Since the preference is on for the current tab, attempting to query the
  // badge text from the extension should return the placeholder text instead of
  // the matched action count.
  queried_badge_text = query_badge_text_from_ext(extension_id, first_tab_id);
  EXPECT_EQ(declarative_net_request::kActionCountPlaceholderBadgeText,
            queried_badge_text);

  // One action was matched, and this should be reflected in the badge text.
  EXPECT_EQ("1", action->GetDisplayBadgeText(first_tab_id));

  SetActionsAsBadgeText(extension_id, false);
  // Switching the preference off should cause the extension queried badge text
  // to be the explicitly set badge text for this tab if it exists. In this
  // case, the queried badge text should be the default badge text.
  queried_badge_text = query_badge_text_from_ext(extension_id, first_tab_id);
  EXPECT_EQ(default_badge_text, queried_badge_text);

  // The displayed badge text should be the default badge text now that the
  // preference is off.
  EXPECT_EQ(default_badge_text, action->GetDisplayBadgeText(first_tab_id));

  // Verify that turning off the preference deletes the DNR action count within
  // the extension action.
  EXPECT_FALSE(action->HasDNRActionCount(first_tab_id));
}

// Test that enabling the setActionCountAsBadgeText preference will update
// all browsers sharing the same browser context.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ActionCountPreferenceMultipleWindows) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = "abc.com";
  rule.id = kMinValidID;
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.action->type = "block";

  std::vector<TestRule> rules({rule});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rules}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const ExtensionId& extension_id = last_loaded_extension_id();
  const Extension* dnr_extension = last_loaded_extension();

  ExtensionAction* extension_action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  const GURL page_url = embedded_test_server()->GetURL(
      "abc.com", "/pages_with_script/index.html");
  ui_test_utils::NavigateToURL(browser(), page_url);

  int first_browser_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  EXPECT_EQ("", extension_action->GetDisplayBadgeText(first_browser_tab_id));

  // Now create a new browser with the same profile as |browser()| and navigate
  // to |page_url|.
  Browser* second_browser = CreateBrowser(profile());
  ui_test_utils::NavigateToURL(second_browser, page_url);
  content::WebContents* second_browser_contents =
      second_browser->tab_strip_model()->GetActiveWebContents();

  int second_browser_tab_id =
      ExtensionTabUtil::GetTabId(second_browser_contents);
  EXPECT_EQ("", extension_action->GetDisplayBadgeText(second_browser_tab_id));

  // Set up an observer to listen for ExtensionAction updates for the active web
  // contents of both browser windows.
  TestExtensionActionAPIObserver test_api_observer(
      profile(), extension_id, {web_contents(), second_browser_contents});

  SetActionsAsBadgeText(extension_id, true);

  // Wait until ExtensionActionAPI::NotifyChange is called, then perform a
  // sanity check that one action was matched, and this is reflected in the
  // badge text.
  test_api_observer.Wait();

  EXPECT_EQ("1", extension_action->GetDisplayBadgeText(first_browser_tab_id));

  // The badge text for the second browser window should also update to the
  // matched action count because the second browser shares the same browser
  // context as the first.
  EXPECT_EQ("1", extension_action->GetDisplayBadgeText(second_browser_tab_id));
}

// Test that the action matched badge text for an extension is visible in an
// incognito context if the extension is incognito enabled.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ActionsMatchedCountAsBadgeTextIncognito) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = "abc.com";
  rule.id = kMinValidID;
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.action->type = "block";

  std::vector<TestRule> rules({rule});
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {rules}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const ExtensionId extension_id = last_loaded_extension_id();
  util::SetIsIncognitoEnabled(extension_id, profile(), true /*enabled*/);

  ExtensionPrefs::Get(profile())->SetDNRUseActionCountAsBadgeText(extension_id,
                                                                  true);

  Browser* incognito_browser = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito_browser, GURL("http://abc.com"));

  content::WebContents* incognito_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  const Extension* dnr_extension = last_loaded_extension();
  ExtensionAction* incognito_action =
      ExtensionActionManager::Get(incognito_contents->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  EXPECT_EQ("1", incognito_action->GetDisplayBadgeText(
                     ExtensionTabUtil::GetTabId(incognito_contents)));
}

// Test that the actions matched badge text for an extension will be reset
// when a main-frame navigation finishes.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       ActionsMatchedCountAsBadgeTextMainFrame) {
  auto get_url_for_host = [this](std::string hostname) {
    return embedded_test_server()->GetURL(hostname,
                                          "/pages_with_script/index.html");
  };

  struct {
    std::string url_filter;
    int id;
    int priority;
    std::string action_type;
    std::vector<std::string> resource_types;
    base::Optional<std::string> redirect_url;
  } rules_data[] = {
      {"abc.com", 1, 1, "block", std::vector<std::string>({"script"}),
       base::nullopt},
      {"||def.com", 2, 1, "redirect", std::vector<std::string>({"main_frame"}),
       get_url_for_host("abc.com").spec()},
      {"gotodef.com", 3, 1, "redirect",
       std::vector<std::string>({"main_frame"}),
       get_url_for_host("def.com").spec()},
      {"ghi.com", 4, 1, "block", std::vector<std::string>({"main_frame"}),
       base::nullopt},
  };

  // Load the extension.
  std::vector<TestRule> rules;
  for (const auto& rule_data : rules_data) {
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = rule_data.url_filter;
    rule.id = rule_data.id;
    rule.priority = rule_data.priority;
    rule.condition->resource_types = rule_data.resource_types;
    rule.action->type = rule_data.action_type;
    rule.action->redirect.emplace();
    rule.action->redirect->url = rule_data.redirect_url;
    rules.push_back(rule);
  }

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      rules, "test_extension", {URLPattern::kAllUrlsPattern}));

  const Extension* dnr_extension = last_loaded_extension();

  ExtensionPrefs::Get(profile())->SetDNRUseActionCountAsBadgeText(
      last_loaded_extension_id(), true);

  ExtensionAction* action =
      ExtensionActionManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionAction(*dnr_extension);

  struct {
    std::string frame_hostname;
    std::string expected_badge_text;
  } test_cases[] = {
      // The request to ghi.com should be blocked, so the badge text should be 1
      // once navigation finishes.
      {"ghi.com", "1"},
      // The script on get_url_for_host("abc.com") matches with a rule and
      // should increment the badge text.
      {"abc.com", "1"},
      // No rules match, so there should be no badge text once navigation
      // finishes.
      {"nomatch.com", ""},
      // The request to def.com will redirect to get_url_for_host("abc.com") and
      // the script on abc.com should match with a rule.
      {"def.com", "2"},
      // Same as the above test, except with an additional redirect from
      // gotodef.com to def.com caused by a rule match. Therefore the badge text
      // should be 3.
      {"gotodef.com", "3"},
  };

  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  for (const auto& test_case : test_cases) {
    GURL url = get_url_for_host(test_case.frame_hostname);
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_case.expected_badge_text,
              action->GetDisplayBadgeText(first_tab_id));
  }
}

// Test that the onRuleMatchedDebug event is only available for unpacked
// extensions.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       OnRuleMatchedDebugAvailability) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it can have access to the onRuleMatchedDebug event when
  // unpacked.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
      {}, "test_extension", {URLPattern::kAllUrlsPattern}));

  const char kGetOnRuleMatchedDebugScript[] = R"(
    const hasEvent = !!chrome.declarativeNetRequest.onRuleMatchedDebug ?
      'true' : 'false';
    window.domAutomationController.send(hasEvent);
  )";
  std::string actual_event_availability = ExecuteScriptInBackgroundPage(
      last_loaded_extension_id(), kGetOnRuleMatchedDebugScript);

  std::string expected_event_availability =
      GetParam() == ExtensionLoadType::UNPACKED ? "true" : "false";

  ASSERT_EQ(expected_event_availability, actual_event_availability);
}

// Test that getMatchedRules returns the correct rules when called by different
// extensions with rules matched by requests initiated from different tabs.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetMatchedRulesMultipleTabs) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it has access to the getMatchedRules API function.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  // Sub-frames are used for navigations instead of the main-frame to allow
  // multiple requests to be made without triggering a main-frame navigation,
  // which would move rules attributed to the previous main-frame to the unknown
  // tab ID.
  const std::string kFrameName1 = "frame1";
  const GURL page_url = embedded_test_server()->GetURL(
      "nomatch.com", "/page_with_two_frames.html");

  auto get_url_for_host = [this](std::string hostname) {
    return embedded_test_server()->GetURL(hostname,
                                          "/pages_with_script/index.html");
  };

  auto create_block_rule = [](int id, const std::string& url_filter) {
    TestRule rule = CreateGenericRule();
    rule.id = id;
    rule.condition->url_filter = url_filter;
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rule.action->type = "block";
    return rule;
  };

  TestRule abc_rule = create_block_rule(kMinValidID, "abc.com");
  TestRule def_rule = create_block_rule(kMinValidID + 1, "def.com");

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({abc_rule}, "extension_1", {}));
  auto extension_1_id = last_loaded_extension_id();

  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRules({def_rule}, "extension_2", {}));
  auto extension_2_id = last_loaded_extension_id();

  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  const int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());

  // Navigate to abc.com. The rule from |extension_1| should match for
  // |first_tab_id|.
  NavigateFrame(kFrameName1, get_url_for_host("abc.com"));

  // Navigate to abc.com. The rule from |extension_2| should match for
  // |first_tab_id|.
  NavigateFrame(kFrameName1, get_url_for_host("def.com"));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));
  const int second_tab_id = ExtensionTabUtil::GetTabId(web_contents());

  // Navigate to abc.com from the second tab. The rule from |extension_1| should
  // match for |second_tab_id|.
  NavigateFrame(kFrameName1, get_url_for_host("abc.com"));

  int abc_id = *abc_rule.id;
  int def_id = *def_rule.id;

  struct {
    ExtensionId extension_id;
    base::Optional<int> tab_id;
    std::string expected_rule_and_tab_ids;
  } test_cases[] = {
      // No rules should be matched for |extension_1| and the unknown tab ID.
      {extension_1_id, extension_misc::kUnknownTabId, ""},

      // No filter is specified for |extension_1|, therefore two MatchedRuleInfo
      // should be returned:
      // (abc_id, first_tab_id) and (abc_id, second_tab_id)
      {extension_1_id, base::nullopt,
       base::StringPrintf("%d,%d|%d,%d", abc_id, first_tab_id, abc_id,
                          second_tab_id)},

      // Filtering by tab_id = |first_tab_id| should return one MatchedRuleInfo:
      // (abc_id, first_tab_id)
      {extension_1_id, first_tab_id,
       base::StringPrintf("%d,%d", abc_id, first_tab_id)},

      // Filtering by tab_id = |second_tab_id| should return one
      // MatchedRuleInfo: (abc_id, second_tab_id)
      {extension_1_id, second_tab_id,
       base::StringPrintf("%d,%d", abc_id, second_tab_id)},

      // For |extension_2|, filtering by tab_id = |first_tab_id| should return
      // one MatchedRuleInfo: (def_id, first_tab_id)
      {extension_2_id, first_tab_id,
       base::StringPrintf("%d,%d", def_id, first_tab_id)},

      // Since no rules from |extension_2| was matched for the second tab,
      // getMatchedRules should not return any rules.
      {extension_2_id, second_tab_id, ""}};

  for (const auto& test_case : test_cases) {
    if (test_case.tab_id) {
      SCOPED_TRACE(base::StringPrintf(
          "Testing getMatchedRules for tab %d and extension %s",
          *test_case.tab_id, test_case.extension_id.c_str()));
    } else {
      SCOPED_TRACE(base::StringPrintf(
          "Testing getMatchedRules for all tabs and extension %s",
          test_case.extension_id.c_str()));
    }

    std::string actual_rule_and_tab_ids =
        GetRuleAndTabIdsMatched(test_case.extension_id, test_case.tab_id);
    EXPECT_EQ(test_case.expected_rule_and_tab_ids, actual_rule_and_tab_ids);
  }

  // Close the second tab opened.
  browser()->tab_strip_model()->CloseSelectedTabs();

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(0));

  std::string actual_rule_and_tab_ids =
      GetRuleAndTabIdsMatched(extension_1_id, base::nullopt);

  // The matched rule info for the second tab should have its tab ID changed to
  // the unknown tab ID after the second tab has been closed.
  EXPECT_EQ(
      base::StringPrintf("%d,%d|%d,%d", abc_id, extension_misc::kUnknownTabId,
                         abc_id, first_tab_id),
      actual_rule_and_tab_ids);
}

// Test that rules matched for main-frame navigations are attributed will be
// reset when a main-frame navigation finishes.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetMatchedRulesMainFrame) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it has access to the getMatchedRules API function.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  const std::string test_host = "abc.com";
  GURL page_url = embedded_test_server()->GetURL(
      test_host, "/pages_with_script/index.html");

  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  rule.condition->url_filter = test_host;
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.action->type = "block";

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}, "extension_1", {}));

  // Navigate to abc.com.
  ui_test_utils::NavigateToURL(browser(), page_url);
  std::string actual_rule_and_tab_ids =
      GetRuleAndTabIdsMatched(last_loaded_extension_id(), base::nullopt);

  // Since the block rule for abc.com is matched for the main-frame navigation
  // request, it should be attributed to the current tab.
  const int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());
  std::string expected_rule_and_tab_ids =
      base::StringPrintf("%d,%d", *rule.id, first_tab_id);

  EXPECT_EQ(expected_rule_and_tab_ids, actual_rule_and_tab_ids);

  // Navigate to abc.com again.
  ui_test_utils::NavigateToURL(browser(), page_url);
  actual_rule_and_tab_ids =
      GetRuleAndTabIdsMatched(last_loaded_extension_id(), base::nullopt);

  // The same block rule is matched for this navigation request, and should be
  // attributed to the current tab. Since the main-frame for which the older
  // matched rule is associated with is no longer active, the older matched
  // rule's tab ID should be changed to the unknown tab ID.
  expected_rule_and_tab_ids =
      base::StringPrintf("%d,%d|%d,%d", *rule.id, extension_misc::kUnknownTabId,
                         *rule.id, first_tab_id);

  EXPECT_EQ(expected_rule_and_tab_ids, actual_rule_and_tab_ids);

  // Navigate to nomatch,com.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "nomatch.com", "/pages_with_script/index.html"));

  // No rules should be matched for the navigation request to nomatch.com and
  // all rules previously attributed to |first_tab_id| should now be attributed
  // to the unknown tab ID as a result of the navigation. Therefore
  // GetMatchedRules should return no matched rules.
  actual_rule_and_tab_ids =
      GetRuleAndTabIdsMatched(last_loaded_extension_id(), first_tab_id);
  EXPECT_EQ("", actual_rule_and_tab_ids);
}

// Test that getMatchedRules only returns rules more recent than the provided
// timestamp.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetMatchedRulesTimestampFiltering) {
  base::Time start_time = base::Time::Now();

  base::SimpleTestClock clock_;
  clock_.SetNow(start_time);
  rules_monitor_service()->action_tracker().SetClockForTests(&clock_);

  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it has access to the getMatchedRules API function.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  const std::string kFrameName1 = "frame1";
  const GURL page_url = embedded_test_server()->GetURL(
      "nomatch.com", "/page_with_two_frames.html");

  const std::string example_host = "example.com";
  const GURL sub_frame_url = embedded_test_server()->GetURL(
      example_host, "/pages_with_script/index.html");

  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  rule.condition->url_filter = example_host;
  rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
  rule.action->type = "block";

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}, "extension_1", {}));
  const ExtensionId& extension_id = last_loaded_extension_id();
  ui_test_utils::NavigateToURL(browser(), page_url);

  // Using subframes here to make requests without triggering main-frame
  // navigations. This request will match with the block rule for example.com at
  // |start_time|.
  NavigateFrame(kFrameName1, sub_frame_url);

  const char getMatchedRuleTimestampScript[] = R"(
    chrome.declarativeNetRequest.getMatchedRules((rules) => {
      var rule_count = rules.rulesMatchedInfo.length;
      var timestamp = rule_count === 1 ?
          rules.rulesMatchedInfo[0].timeStamp.toString() : '';

      window.domAutomationController.send(timestamp);
    });
  )";

  std::string timestamp_string = ExecuteScriptInBackgroundPage(
      extension_id, getMatchedRuleTimestampScript,
      browsertest_util::ScriptUserActivation::kDontActivate);

  double matched_rule_timestamp;
  ASSERT_TRUE(base::StringToDouble(timestamp_string, &matched_rule_timestamp));

  // Verify that the rule was matched at |start_time|.
  EXPECT_DOUBLE_EQ(start_time.ToJsTimeIgnoringNull(), matched_rule_timestamp);

  // Advance the clock to capture a timestamp after when the first request was
  // made.
  clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  base::Time timestamp_1 = clock_.Now();
  clock_.Advance(base::TimeDelta::FromMilliseconds(100));

  // Navigate to example.com again. This should cause |rule| to be matched.
  NavigateFrame(kFrameName1, sub_frame_url);

  // Advance the clock to capture a timestamp after when the second request was
  // made.
  clock_.Advance(base::TimeDelta::FromMilliseconds(100));
  base::Time timestamp_2 = clock_.Now();

  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());

  // Two rules should be matched on |first_tab_id|.
  std::string rule_count = GetMatchedRuleCount(extension_id, first_tab_id,
                                               base::nullopt /* timestamp */);
  EXPECT_EQ("2", rule_count);

  // Only one rule should be matched on |first_tab_id| after |timestamp_1|.
  rule_count = GetMatchedRuleCount(extension_id, first_tab_id, timestamp_1);
  EXPECT_EQ("1", rule_count);

  // No rules should be matched on |first_tab_id| after |timestamp_2|.
  rule_count = GetMatchedRuleCount(extension_id, first_tab_id, timestamp_2);
  EXPECT_EQ("0", rule_count);

  rules_monitor_service()->action_tracker().SetClockForTests(nullptr);
}

// Test that getMatchedRules will only return matched rules for individual tabs
// where activeTab is granted.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetMatchedRulesActiveTab) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasActiveTab);

  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(true);

  const std::string test_host = "abc.com";
  GURL page_url = embedded_test_server()->GetURL(
      test_host, "/pages_with_script/index.html");

  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  rule.condition->url_filter = test_host;
  rule.condition->resource_types = std::vector<std::string>({"main_frame"});
  rule.action->type = "block";

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}, "extension_1", {}));
  const ExtensionId& extension_id = last_loaded_extension_id();

  // Navigate to |page_url| which will cause |rule| to be matched.
  ui_test_utils::NavigateToURL(browser(), page_url);

  int first_tab_id = ExtensionTabUtil::GetTabId(web_contents());

  // Open a new tab and navigate to |page_url| which will cause |rule| to be
  // matched.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);

  // Get the ActiveTabPermissionGranter for the second tab.
  ActiveTabPermissionGranter* active_tab_granter =
      tab_helper->active_tab_permission_granter();
  ASSERT_TRUE(active_tab_granter);

  const Extension* dnr_extension = last_loaded_extension();

  // Grant the activeTab permission for the second tab.
  active_tab_granter->GrantIfRequested(dnr_extension);

  int second_tab_id = ExtensionTabUtil::GetTabId(web_contents());

  // Calling getMatchedRules with no tab ID specified should result in an error
  // since the extension does not have the feedback permission.
  EXPECT_EQ(declarative_net_request::kErrorGetMatchedRulesMissingPermissions,
            GetMatchedRuleCount(extension_id, base::nullopt /* tab_id */,
                                base::nullopt /* timestamp */));

  // Calling getMatchedRules for a tab without the activeTab permission granted
  // should result in an error.
  EXPECT_EQ(declarative_net_request::kErrorGetMatchedRulesMissingPermissions,
            GetMatchedRuleCount(extension_id, first_tab_id,
                                base::nullopt /* timestamp */));

  // Calling getMatchedRules for a tab with the activeTab permission granted
  // should return the rules matched for that tab.
  EXPECT_EQ("1", GetMatchedRuleCount(extension_id, second_tab_id,
                                     base::nullopt /* timestamp */));
}

// Test that getMatchedRules will not be throttled if the call is associated
// with a user gesture.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest,
                       GetMatchedRulesNoThrottlingIfUserGesture) {
  // Load the extension with a background script so scripts can be run from its
  // generated background page. Also grant the feedback permission for the
  // extension so it has access to the getMatchedRules API function.
  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript |
                   ConfigFlag::kConfig_HasFeedbackPermission);

  // Ensure that GetMatchedRules is being throttled.
  DeclarativeNetRequestGetMatchedRulesFunction::
      set_disable_throttling_for_tests(false);

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({}));
  const ExtensionId& extension_id = last_loaded_extension_id();

  auto get_matched_rules_count = [this, &extension_id](bool user_gesture) {
    auto user_gesture_setting =
        user_gesture ? browsertest_util::ScriptUserActivation::kActivate
                     : browsertest_util::ScriptUserActivation::kDontActivate;

    return GetMatchedRuleCount(extension_id, base::nullopt /* tab_id */,
                               base::nullopt /* timestamp */,
                               user_gesture_setting);
  };

  // Call getMatchedRules without a user gesture, until the quota is reached.
  // None of these calls should return an error.
  for (int i = 1; i <= dnr_api::MAX_GETMATCHEDRULES_CALLS_PER_INTERVAL; ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "Testing getMatchedRules call without user gesture %d of %d", i,
        dnr_api::MAX_GETMATCHEDRULES_CALLS_PER_INTERVAL));
    EXPECT_EQ("0", get_matched_rules_count(false));
  }

  // Calling getMatchedRules without a user gesture should return an error after
  // the quota has been reached.
  EXPECT_EQ(
      "This request exceeds the MAX_GETMATCHEDRULES_CALLS_PER_INTERVAL quota.",
      get_matched_rules_count(false));

  // Calling getMatchedRules with a user gesture should not return an error even
  // after the quota has been reached.
  EXPECT_EQ("0", get_matched_rules_count(true));
}

// Tests extension update for an extension using declarativeNetRequest.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       ExtensionRemovesOneRulesetOnUpdate) {
  auto create_single_rule_ruleset = [this](
                                        const std::string& ruleset_id_and_path,
                                        bool enabled,
                                        const std::string& filter) {
    std::vector<TestRule> rules = {CreateMainFrameBlockRule(filter)};
    return TestRulesetInfo(ruleset_id_and_path, *ToListValue(rules), enabled);
  };

  set_config_flags(ConfigFlag::kConfig_HasBackgroundScript);

  std::vector<TestRulesetInfo> rulesets = {
      create_single_rule_ruleset("id1", true, "google"),
      create_single_rule_ruleset("id2", false, "yahoo"),
      create_single_rule_ruleset("id3", true, "example"),
  };

  constexpr char kDirectory1[] = "dir1";
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRulesets(rulesets, kDirectory1, {} /* hosts */));
  const ExtensionId extension_id = last_loaded_extension_id();
  const Extension* extension = last_loaded_extension();

  // Also add a dynamic rule.
  ASSERT_NO_FATAL_FAILURE(
      AddDynamicRules(extension_id, {CreateMainFrameBlockRule("dynamic")}));

  // Also update the set of enabled static rulesets.
  ASSERT_NO_FATAL_FAILURE(
      UpdateEnabledRulesets(extension_id, {"id1"}, {"id2", "id3"}));

  CompositeMatcher* composite_matcher =
      ruleset_manager()->GetMatcherForExtension(extension_id);
  ASSERT_TRUE(composite_matcher);
  EXPECT_THAT(GetPublicRulesetIDs(*extension, *composite_matcher),
              UnorderedElementsAre("id2", "id3", dnr_api::DYNAMIC_RULESET_ID));

  // Also sanity check the extension prefs entry for the rulesets.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs);
  int checksum = -1;
  int dynamic_checksum_1 = -1;
  EXPECT_TRUE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, kMinValidStaticRulesetID, &checksum));
  EXPECT_TRUE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 1),
      &checksum));
  EXPECT_TRUE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 2),
      &checksum));
  EXPECT_FALSE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 3),
      &checksum));
  EXPECT_TRUE(
      prefs->GetDNRDynamicRulesetChecksum(extension_id, &dynamic_checksum_1));
  base::Optional<std::set<RulesetID>> enabled_static_rulesets =
      prefs->GetDNREnabledStaticRulesets(extension_id);
  ASSERT_TRUE(enabled_static_rulesets);
  EXPECT_THAT(
      *enabled_static_rulesets,
      UnorderedElementsAre(RulesetID(kMinValidStaticRulesetID.value() + 1),
                           RulesetID(kMinValidStaticRulesetID.value() + 2)));

  std::vector<TestRulesetInfo> new_rulesets = {
      create_single_rule_ruleset("id1", true, "yahoo"),
      create_single_rule_ruleset("new_id2", false, "msn")};

  const char* kDirectory2 = "dir2";
  ASSERT_NO_FATAL_FAILURE(
      UpdateLastLoadedExtension(new_rulesets, kDirectory2, {} /* hosts */,
                                0 /* expected_extension_ruleset_count_change */,
                                true /* has_dynamic_ruleset */));
  extension = extension_registry()->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);

  composite_matcher = ruleset_manager()->GetMatcherForExtension(extension_id);
  ASSERT_TRUE(composite_matcher);

  EXPECT_THAT(GetPublicRulesetIDs(*extension, *composite_matcher),
              UnorderedElementsAre("id1", dnr_api::DYNAMIC_RULESET_ID));

  int dynamic_checksum_2;
  EXPECT_TRUE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, kMinValidStaticRulesetID, &checksum));
  EXPECT_TRUE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 1),
      &checksum));
  EXPECT_FALSE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 2),
      &checksum));
  EXPECT_TRUE(
      prefs->GetDNRDynamicRulesetChecksum(extension_id, &dynamic_checksum_2));
  EXPECT_EQ(dynamic_checksum_2, dynamic_checksum_1);

  // Ensure the preference for enabled static rulesets is cleared on extension
  // update.
  EXPECT_FALSE(prefs->GetDNREnabledStaticRulesets(extension_id));
}

// Tests extension update for an extension using declarativeNetRequest.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest_Packed,
                       ExtensionRemovesAllRulesetsOnUpdate) {
  auto create_single_rule_ruleset = [this](
                                        const std::string& ruleset_id_and_path,
                                        bool enabled,
                                        const std::string& filter) {
    std::vector<TestRule> rules = {CreateMainFrameBlockRule(filter)};
    return TestRulesetInfo(ruleset_id_and_path, *ToListValue(rules), enabled);
  };

  std::vector<TestRulesetInfo> rulesets = {
      create_single_rule_ruleset("id1", true, "google"),
      create_single_rule_ruleset("id2", true, "example")};

  const char* kDirectory1 = "dir1";
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithRulesets(rulesets, kDirectory1, {} /* hosts */));
  const ExtensionId extension_id = last_loaded_extension_id();
  const Extension* extension = last_loaded_extension();

  CompositeMatcher* composite_matcher =
      ruleset_manager()->GetMatcherForExtension(extension_id);
  ASSERT_TRUE(composite_matcher);

  EXPECT_THAT(GetPublicRulesetIDs(*extension, *composite_matcher),
              UnorderedElementsAre("id1", "id2"));

  // Also sanity check the extension prefs entry for the rulesets.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs);
  int checksum = -1;
  EXPECT_TRUE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, kMinValidStaticRulesetID, &checksum));
  EXPECT_TRUE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 1),
      &checksum));
  EXPECT_FALSE(prefs->GetDNRDynamicRulesetChecksum(extension_id, &checksum));

  const char* kDirectory2 = "dir2";
  ASSERT_NO_FATAL_FAILURE(UpdateLastLoadedExtension(
      {} /* new_rulesets */, kDirectory2, {} /* hosts */,
      -1 /* expected_extension_ruleset_count_change */,
      false /* has_dynamic_ruleset */));
  extension = extension_registry()->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);

  composite_matcher = ruleset_manager()->GetMatcherForExtension(extension_id);
  EXPECT_FALSE(composite_matcher);

  // Ensure the prefs entry are cleared appropriately.
  EXPECT_FALSE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, kMinValidStaticRulesetID, &checksum));
  EXPECT_FALSE(prefs->GetDNRStaticRulesetChecksum(
      extension_id, RulesetID(kMinValidStaticRulesetID.value() + 1),
      &checksum));
  EXPECT_FALSE(prefs->GetDNRDynamicRulesetChecksum(extension_id, &checksum));
}

// Tests the allowAllRequests action.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, AllowAllRequests) {
  struct RuleData {
    int id;
    int priority;
    std::string action_type;
    std::string url_filter;
    bool is_regex_rule;
    base::Optional<std::vector<std::string>> resource_types;
  };

  auto run_test = [this](const std::string& extension_directory,
                         const GURL& page_url,
                         const std::vector<RuleData>& rule_data,
                         const std::vector<std::string>& paths_seen,
                         const std::vector<std::string>& paths_not_seen) {
    std::vector<TestRule> test_rules;
    for (const auto& rule : rule_data) {
      TestRule test_rule = CreateGenericRule();
      test_rule.id = rule.id;
      test_rule.priority = rule.priority;
      test_rule.action->type = rule.action_type;
      test_rule.condition->url_filter.reset();
      if (rule.is_regex_rule)
        test_rule.condition->regex_filter = rule.url_filter;
      else
        test_rule.condition->url_filter = rule.url_filter;
      test_rule.condition->resource_types = rule.resource_types;
      test_rules.push_back(test_rule);
    }

    ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules(
        test_rules, extension_directory, {} /* hosts */));

    ui_test_utils::NavigateToURL(browser(), page_url);

    const std::set<GURL> requests_seen = GetAndResetRequestsToServer();

    for (const auto& path : paths_seen) {
      GURL expected_request_url = embedded_test_server()->GetURL(path);
      EXPECT_TRUE(base::Contains(requests_seen, expected_request_url))
          << expected_request_url.spec()
          << " was not requested from the server.";
    }

    for (const auto& path : paths_not_seen) {
      GURL expected_request_url = embedded_test_server()->GetURL(path);
      EXPECT_FALSE(base::Contains(requests_seen, expected_request_url))
          << expected_request_url.spec() << " request seen unexpectedly.";
    }

    UninstallExtension(last_loaded_extension_id());
  };

  // This page causes the following requests.
  GURL page_url = embedded_test_server()->GetURL("example.com",
                                                 "/page_with_two_frames.html");
  std::vector<std::string> requests = {
      {"/page_with_two_frames.html"},         // 0
      {"/subresources/script.js"},            // 1
      {"/child_frame.html?frame=1"},          // 2
      {"/subresources/script.js?frameId=1"},  // 3
      {"/child_frame.html?frame=2"},          // 4
      {"/subresources/script.js?frameId=2"},  // 5
  };

  {
    SCOPED_TRACE("Testing case 1");
    std::vector<RuleData> rule_data = {
        {1, 4, "allowAllRequests", "page_with_two_frames\\.html", true,
         std::vector<std::string>({"main_frame"})},
        {2, 3, "block", "script.js|", false},
        {3, 5, "block", "script.js?frameId=1", false},
        {4, 3, "block", "script\\.js?frameId=2", true}};
    // Requests:
    // -/page_with_two_frames.html (Matching rule=1)
    //   -/subresources/script.js (Matching rule=[1,2] Winner=1)
    //   -/child_frame.html?frame=1 (Matching Rule=1)
    //     -/subresources/script.js?frameId=1 (Matching Rule=[1,3] Winner=3)
    //   -/child_frame.html?frame=2 (Matching Rule=1)
    //     -/subresources/script.js?frameId=2 (Matching Rule=1,4 Winner=1)
    // Hence only requests[3] is blocked.
    run_test("case_1", page_url, rule_data,
             {requests[0], requests[1], requests[2], requests[4], requests[5]},
             {requests[3]});
  }

  {
    SCOPED_TRACE("Testing case 2");
    std::vector<RuleData> rule_data = {
        {1, 4, "allowAllRequests", "page_with_two_frames.html", false,
         std::vector<std::string>({"main_frame"})},
        {2, 5, "block", "script\\.js", true},
        {3, 6, "allowAllRequests", "child_frame.html", false,
         std::vector<std::string>({"sub_frame"})},
        {4, 7, "block", "frame=1", true}};

    // Requests:
    // -/page_with_two_frames.html (Matching rule=1)
    //   -/subresources/script.js (Matching rule=[1,2] Winner=2, Blocked)
    //   -/child_frame.html?frame=1 (Matching Rule=[1,3,4] Winner=4, Blocked)
    //     -/subresources/script.js?frameId=1 (Source Frame was blocked)
    //   -/child_frame.html?frame=2 (Matching Rule=[1,3] Winner=3)
    //     -/subresources/script.js?frameId=2 (Matching Rule=[1,2,3] Winner=3)
    run_test("case_2", page_url, rule_data,
             {requests[0], requests[4], requests[5]},
             {requests[1], requests[2], requests[3]});
  }

  {
    SCOPED_TRACE("Testing case 3");
    std::vector<RuleData> rule_data = {
        {1, 1, "allowAllRequests", "page_with_two_frames.html", false,
         std::vector<std::string>({"main_frame"})},
        {2, 5, "block", ".*", true},
    };

    // Requests:
    // -/page_with_two_frames.html (Matching rule=1)
    //   -/subresources/script.js (Matching rule=[1,2] Winner=2)
    //   -/child_frame.html?frame=1 (Matching Rule=[1,2] Winner=2)
    //     -/subresources/script.js?frameId=1 (Source Frame was blocked)
    //   -/child_frame.html?frame=2 (Matching Rule=[1,2] Winner=2)
    //     -/subresources/script.js?frameId=2 (Source frame was blocked)
    // Hence only the main-frame request goes through.
    run_test("case_3", page_url, rule_data, {requests[0]},
             {requests[1], requests[2], requests[3], requests[4], requests[5]});
  }
  {
    SCOPED_TRACE("Testing case 4");
    std::vector<RuleData> rule_data = {
        {1, 6, "allowAllRequests", "page_with_two_frames\\.html", true,
         std::vector<std::string>({"main_frame"})},
        {2, 5, "block", "*", false},
    };

    // Requests:
    // -/page_with_two_frames.html (Matching rule=1)
    //   -/subresources/script.js (Matching rule=[1,2] Winner=1)
    //   -/child_frame.html?frame=1 (Matching Rule=[1,2] Winner=1)
    //     -/subresources/script.js?frameId=1 (Matching Rule=[1,2] Winner=1)
    //   -/child_frame.html?frame=2 (Matching Rule=[1,2] Winner=1)
    //     -/subresources/script.js?frameId=2 (Matching Rule=[1,2] Winner=1)
    // Hence all requests go through.
    run_test("case_4", page_url, rule_data,
             {requests[0], requests[1], requests[2], requests[3], requests[4],
              requests[5]},
             {});
  }
}

// Test fixture to verify that host permissions for the request url and the
// request initiator are properly checked when redirecting requests. Loads an
// example.com url with four sub-frames named frame_[1..4] from hosts
// frame_[1..4].com. These subframes point to invalid sources. The initiator for
// these frames will be example.com. Loads an extension which redirects these
// sub-frames to a valid source. Verifies that the correct frames are redirected
// depending on the host permissions for the extension.
class DeclarativeNetRequestHostPermissionsBrowserTest
    : public DeclarativeNetRequestBrowserTest {
 public:
  DeclarativeNetRequestHostPermissionsBrowserTest() {}

 protected:
  struct FrameRedirectResult {
    std::string child_frame_name;
    bool expect_frame_redirected;
  };

  void LoadExtensionWithHostPermissions(const std::vector<std::string>& hosts) {
    TestRule rule = CreateGenericRule();
    rule.priority = kMinValidPriority;
    rule.condition->url_filter = std::string("not_a_valid_child_frame.html");
    rule.condition->resource_types = std::vector<std::string>({"sub_frame"});
    rule.action->type = std::string("redirect");
    rule.action->redirect.emplace();
    rule.action->redirect->url =
        embedded_test_server()->GetURL("foo.com", "/child_frame.html").spec();

    ASSERT_NO_FATAL_FAILURE(
        LoadExtensionWithRules({rule}, "test_extension", hosts));
  }

  void RunTests(const std::vector<FrameRedirectResult>& expected_results) {
    ASSERT_EQ(4u, expected_results.size());

    GURL url = embedded_test_server()->GetURL("example.com",
                                              "/page_with_four_frames.html");
    ui_test_utils::NavigateToURL(browser(), url);
    ASSERT_TRUE(WasFrameWithScriptLoaded(GetMainFrame()));

    for (const auto& frame_result : expected_results) {
      SCOPED_TRACE(base::StringPrintf("Testing child frame named %s",
                                      frame_result.child_frame_name.c_str()));

      content::RenderFrameHost* child =
          GetFrameByName(frame_result.child_frame_name);
      ASSERT_TRUE(child);
      EXPECT_EQ(frame_result.expect_frame_redirected,
                WasFrameWithScriptLoaded(child));
    }
  }

  std::string GetMatchPatternForDomain(const std::string& domain) const {
    return "*://*." + domain + ".com/*";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestHostPermissionsBrowserTest);
};

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestHostPermissionsBrowserTest,
                       AllURLs1) {
  // All frames should be redirected since the extension has access to all
  // hosts.
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithHostPermissions({"<all_urls>"}));
  RunTests({{"frame_1", true},
            {"frame_2", true},
            {"frame_3", true},
            {"frame_4", true}});
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestHostPermissionsBrowserTest,
                       NoPermissions) {
  // The extension has no host permissions. No frames should be redirected.
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithHostPermissions({}));
  RunTests({{"frame_1", false},
            {"frame_2", false},
            {"frame_3", false},
            {"frame_4", false}});
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestHostPermissionsBrowserTest,
                       SubframesRequireNoInitiatorPermissions) {
  // The extension has access to requests to "frame_1.com" and "frame_2.com".
  // These should be redirected. Note: extensions don't need access to the
  // initiator of a navigation request to redirect it (See crbug.com/918137).
  ASSERT_NO_FATAL_FAILURE(
      LoadExtensionWithHostPermissions({GetMatchPatternForDomain("frame_1"),
                                        GetMatchPatternForDomain("frame_2")}));
  RunTests({{"frame_1", true},
            {"frame_2", true},
            {"frame_3", false},
            {"frame_4", false}});
}

// Fixture to test the "resourceTypes" and "excludedResourceTypes" fields of a
// declarative rule condition.
class DeclarativeNetRequestResourceTypeBrowserTest
    : public DeclarativeNetRequestBrowserTest {
 public:
  DeclarativeNetRequestResourceTypeBrowserTest() {}

 protected:
  // TODO(crbug.com/696822): Add tests for "object", "ping", "other", "font",
  // "csp_report".
  enum ResourceTypeMask {
    kNone = 0,
    kSubframe = 1 << 0,
    kStylesheet = 1 << 1,
    kScript = 1 << 2,
    kImage = 1 << 3,
    kXHR = 1 << 4,
    kMedia = 1 << 5,
    kWebSocket = 1 << 6,
    kAll = (1 << 7) - 1
  };

  struct TestCase {
    std::string hostname;
    int blocked_mask;
  };

  void RunTests(const std::vector<TestCase>& test_cases) {
    // Start a web socket test server to test the websocket resource type.
    net::SpawnedTestServer websocket_test_server(
        net::SpawnedTestServer::TYPE_WS, net::GetWebSocketTestDataDirectory());
    ASSERT_TRUE(websocket_test_server.Start());

    // The |websocket_url| will echo the message we send to it.
    GURL websocket_url = websocket_test_server.GetURL("echo-with-no-extension");

    auto execute_script = [](content::RenderFrameHost* frame,
                             const std::string& script) {
      bool subresource_loaded = false;
      EXPECT_TRUE(content::ExecuteScriptAndExtractBool(frame, script,
                                                       &subresource_loaded));
      return subresource_loaded;
    };

    for (const auto& test_case : test_cases) {
      GURL url = embedded_test_server()->GetURL(test_case.hostname,
                                                "/subresources.html");
      SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

      ui_test_utils::NavigateToURL(browser(), url);
      ASSERT_EQ(content::PAGE_TYPE_NORMAL, GetPageType());

      content::RenderFrameHost* frame = GetMainFrame();

      // sub-frame.
      EXPECT_EQ(
          !(test_case.blocked_mask & kSubframe),
          execute_script(
              frame, "domAutomationController.send(!!window.frameLoaded);"));

      // stylesheet
      EXPECT_EQ(!(test_case.blocked_mask & kStylesheet),
                execute_script(frame, "testStylesheet();"));

      // script
      EXPECT_EQ(!(test_case.blocked_mask & kScript),
                execute_script(frame, "testScript();"));

      // image
      EXPECT_EQ(!(test_case.blocked_mask & kImage),
                execute_script(frame, "testImage();"));

      // xhr
      EXPECT_EQ(!(test_case.blocked_mask & kXHR),
                execute_script(frame, "testXHR();"));

      // media
      EXPECT_EQ(!(test_case.blocked_mask & kMedia),
                execute_script(frame, "testMedia();"));

      // websocket
      EXPECT_EQ(!(test_case.blocked_mask & kWebSocket),
                execute_script(
                    frame, base::StringPrintf("testWebSocket('%s');",
                                              websocket_url.spec().c_str())));
    }
  }

  // Loads an extension to test blocking different resource types.
  void LoadExtension() {
    struct {
      std::string domain;
      size_t id;
      std::vector<std::string> resource_types;
      std::vector<std::string> excluded_resource_types;
    } rules_data[] = {
        {"block_subframe.com", 1, {"sub_frame"}, {}},
        {"block_stylesheet.com", 2, {"stylesheet"}, {}},
        {"block_script.com", 3, {"script"}, {}},
        {"block_image.com", 4, {"image"}, {}},
        {"block_xhr.com", 5, {"xmlhttprequest"}, {}},
        {"block_media.com", 6, {"media"}, {}},
        {"block_websocket.com", 7, {"websocket"}, {}},
        {"block_image_and_stylesheet.com", 8, {"image", "stylesheet"}, {}},
        {"block_subframe_and_xhr.com", 11, {"sub_frame", "xmlhttprequest"}, {}},
        {"block_all.com", 9, {}, {}},
        {"block_all_but_xhr_and_script.com",
         10,
         {},
         {"xmlhttprequest", "script"}},
    };

    std::vector<TestRule> rules;
    for (const auto& rule_data : rules_data) {
      TestRule rule = CreateGenericRule();

      // The "resourceTypes" property (i.e. |rule.condition->resource_types|)
      // should not be an empty list. It should either be omitted or be a non-
      // empty list.
      if (rule_data.resource_types.empty())
        rule.condition->resource_types = base::nullopt;
      else
        rule.condition->resource_types = rule_data.resource_types;

      rule.condition->excluded_resource_types =
          rule_data.excluded_resource_types;
      rule.id = rule_data.id;
      rule.condition->domains = std::vector<std::string>({rule_data.domain});
      // Don't specify the urlFilter, which should behaves the same as "*".
      rule.condition->url_filter = base::nullopt;
      rules.push_back(rule);
    }
    LoadExtensionWithRules(rules);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestResourceTypeBrowserTest);
};

// These are split into two tests to prevent a timeout. See crbug.com/787957.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestResourceTypeBrowserTest, Test1) {
  ASSERT_NO_FATAL_FAILURE(LoadExtension());
  RunTests({{"block_subframe.com", kSubframe},
            {"block_stylesheet.com", kStylesheet},
            {"block_script.com", kScript},
            {"block_image.com", kImage},
            {"block_xhr.com", kXHR},
            {"block_media.com", kMedia}});
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestResourceTypeBrowserTest, Test2) {
  ASSERT_NO_FATAL_FAILURE(LoadExtension());
  RunTests({{"block_websocket.com", kWebSocket},
            {"block_image_and_stylesheet.com", kImage | kStylesheet},
            {"block_subframe_and_xhr.com", kSubframe | kXHR},
            {"block_all.com", kAll},
            {"block_all_but_xhr_and_script.com", kAll & ~kXHR & ~kScript},
            {"block_none.com", kNone}});
}

INSTANTIATE_TEST_SUITE_P(All,
                         DeclarativeNetRequestBrowserTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

INSTANTIATE_TEST_SUITE_P(All,
                         DeclarativeNetRequestHostPermissionsBrowserTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));
INSTANTIATE_TEST_SUITE_P(All,
                         DeclarativeNetRequestResourceTypeBrowserTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

INSTANTIATE_TEST_SUITE_P(All,
                         DeclarativeNetRequestBrowserTest_Packed,
                         ::testing::Values(ExtensionLoadType::PACKED));

INSTANTIATE_TEST_SUITE_P(All,
                         DeclarativeNetRequestBrowserTest_Unpacked,
                         ::testing::Values(ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
