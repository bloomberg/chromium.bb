// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/file_sequence_helper.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_service_factory.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/extension_id.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace dnr_api = api::declarative_net_request;

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<RulesMonitorService>>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

void LoadRulesetOnIOThread(ExtensionId extension_id,
                           std::unique_ptr<CompositeMatcher> matcher,
                           URLPatternSet allowed_pages,
                           InfoMap* info_map,
                           void* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(info_map);

  RulesetManager* manager = info_map->GetRulesetManager();
  bool increment_extra_headers = !manager->HasAnyExtraHeadersMatcher() &&
                                 matcher->HasAnyExtraHeadersMatcher();
  manager->AddRuleset(extension_id, std::move(matcher),
                      std::move(allowed_pages));

  if (increment_extra_headers) {
    ExtensionWebRequestEventRouter::GetInstance()
        ->IncrementExtraHeadersListenerCount(browser_context);
  }
}

void UnloadRulesetOnIOThread(ExtensionId extension_id,
                             InfoMap* info_map,
                             void* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(info_map);

  RulesetManager* manager = info_map->GetRulesetManager();
  bool had_extra_headers_matcher = manager->HasAnyExtraHeadersMatcher();
  info_map->GetRulesetManager()->RemoveRuleset(extension_id);

  if (had_extra_headers_matcher && !manager->HasAnyExtraHeadersMatcher()) {
    ExtensionWebRequestEventRouter::GetInstance()
        ->DecrementExtraHeadersListenerCount(browser_context);
  }
}

void UpdateRulesetMatcherOnIOThread(
    ExtensionId extension_id,
    std::unique_ptr<RulesetMatcher> ruleset_matcher,
    InfoMap* info_map,
    void* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(info_map);

  RulesetManager* manager = info_map->GetRulesetManager();
  bool had_extra_headers_matcher = manager->HasAnyExtraHeadersMatcher();

  CompositeMatcher* matcher =
      info_map->GetRulesetManager()->GetMatcherForExtension(extension_id);
  DCHECK(matcher);
  matcher->AddOrUpdateRuleset(std::move(ruleset_matcher));

  bool has_extra_headers_matcher = manager->HasAnyExtraHeadersMatcher();
  if (had_extra_headers_matcher == has_extra_headers_matcher)
    return;
  if (has_extra_headers_matcher) {
    ExtensionWebRequestEventRouter::GetInstance()
        ->IncrementExtraHeadersListenerCount(browser_context);
  } else {
    ExtensionWebRequestEventRouter::GetInstance()
        ->DecrementExtraHeadersListenerCount(browser_context);
  }
}

}  // namespace

// Helper to bridge tasks to FileSequenceHelper. Lives on the UI thread.
class RulesMonitorService::FileSequenceBridge {
 public:
  FileSequenceBridge()
      : file_task_runner_(GetExtensionFileTaskRunner()),
        file_sequence_helper_(std::make_unique<FileSequenceHelper>()) {}

  ~FileSequenceBridge() {
    file_task_runner_->DeleteSoon(FROM_HERE, std::move(file_sequence_helper_));
  }

  void LoadRulesets(
      LoadRequestData load_data,
      FileSequenceHelper::LoadRulesetsUICallback ui_callback) const {
    // base::Unretained is safe here because we trigger the destruction of
    // |file_sequence_helper_| on |file_task_runner_| from our destructor. Hence
    // it is guaranteed to be alive when |load_ruleset_task| is run.
    base::OnceClosure load_ruleset_task =
        base::BindOnce(&FileSequenceHelper::LoadRulesets,
                       base::Unretained(file_sequence_helper_.get()),
                       std::move(load_data), std::move(ui_callback));
    file_task_runner_->PostTask(FROM_HERE, std::move(load_ruleset_task));
  }

  void UpdateDynamicRules(
      LoadRequestData load_data,
      std::vector<dnr_api::Rule> rules,
      DynamicRuleUpdateAction action,
      FileSequenceHelper::UpdateDynamicRulesUICallback ui_callback) const {
    // base::Unretained is safe here because we trigger the destruction of
    // |file_sequence_state_| on |file_task_runner_| from our destructor. Hence
    // it is guaranteed to be alive when |update_dynamic_rules_task| is run.
    base::OnceClosure update_dynamic_rules_task = base::BindOnce(
        &FileSequenceHelper::UpdateDynamicRules,
        base::Unretained(file_sequence_helper_.get()), std::move(load_data),
        std::move(rules), action, std::move(ui_callback));
    file_task_runner_->PostTask(FROM_HERE,
                                std::move(update_dynamic_rules_task));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Created on the UI thread. Accessed and destroyed on |file_task_runner_|.
  // Maintains state needed on |file_task_runner_|.
  std::unique_ptr<FileSequenceHelper> file_sequence_helper_;

  DISALLOW_COPY_AND_ASSIGN(FileSequenceBridge);
};

// static
BrowserContextKeyedAPIFactory<RulesMonitorService>*
RulesMonitorService::GetFactoryInstance() {
  return g_factory.Pointer();
}

bool RulesMonitorService::HasAnyRegisteredRulesets() const {
  return !extensions_with_rulesets_.empty();
}

bool RulesMonitorService::HasRegisteredRuleset(
    const ExtensionId& extension_id) const {
  return extensions_with_rulesets_.find(extension_id) !=
         extensions_with_rulesets_.end();
}

void RulesMonitorService::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void RulesMonitorService::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void RulesMonitorService::UpdateDynamicRules(
    const Extension& extension,
    std::vector<api::declarative_net_request::Rule> rules,
    DynamicRuleUpdateAction action,
    DynamicRuleUpdateUICallback callback) {
  DCHECK(HasRegisteredRuleset(extension.id()));

  LoadRequestData data(extension.id());

  // We are updating the indexed ruleset. Don't set the expected checksum since
  // it'll change.
  data.rulesets.emplace_back(RulesetSource::CreateDynamic(context_, extension));

  auto update_rules_callback =
      base::BindOnce(&RulesMonitorService::OnDynamicRulesUpdated,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  file_sequence_bridge_->UpdateDynamicRules(std::move(data), std::move(rules),
                                            action,
                                            std::move(update_rules_callback));
}

RulesMonitorService::RulesMonitorService(
    content::BrowserContext* browser_context)
    : registry_observer_(this),
      file_sequence_bridge_(std::make_unique<FileSequenceBridge>()),
      info_map_(ExtensionSystem::Get(browser_context)->info_map()),
      prefs_(ExtensionPrefs::Get(browser_context)),
      extension_registry_(ExtensionRegistry::Get(browser_context)),
      warning_service_(WarningService::Get(browser_context)),
      context_(browser_context),
      weak_factory_(this) {
  registry_observer_.Add(extension_registry_);
}

RulesMonitorService::~RulesMonitorService() = default;

/* Description of thread hops for various scenarios:

   On ruleset load success:
      - UI -> File -> UI -> IO.
      - The File sequence might reindex the ruleset while parsing JSON OOP.

   On ruleset load failure:
      - UI -> File -> UI.
      - The File sequence might reindex the ruleset while parsing JSON OOP.

   On ruleset unload:
      - UI -> IO.

   On dynamic rules update.
                            |-> IPC to extension
      - UI -> File -> UI -> |
                            |-> IO
*/

void RulesMonitorService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK_EQ(context_, browser_context);

  if (!declarative_net_request::DNRManifestData::HasRuleset(*extension))
    return;

  DCHECK(IsAPIAvailable());

  LoadRequestData load_data(extension->id());
  int expected_ruleset_checksum;

  // Static ruleset.
  {
    bool has_checksum = prefs_->GetDNRRulesetChecksum(
        extension->id(), &expected_ruleset_checksum);
    DCHECK(has_checksum);

    RulesetInfo static_ruleset(RulesetSource::CreateStatic(*extension));
    static_ruleset.set_expected_checksum(expected_ruleset_checksum);
    load_data.rulesets.push_back(std::move(static_ruleset));
  }

  // Dynamic ruleset
  if (prefs_->GetDNRDynamicRulesetChecksum(extension->id(),
                                           &expected_ruleset_checksum)) {
    RulesetInfo dynamic_ruleset(
        RulesetSource::CreateDynamic(browser_context, *extension));
    dynamic_ruleset.set_expected_checksum(expected_ruleset_checksum);
    load_data.rulesets.push_back(std::move(dynamic_ruleset));
  }

  auto load_ruleset_callback = base::BindOnce(
      &RulesMonitorService::OnRulesetLoaded, weak_factory_.GetWeakPtr());
  file_sequence_bridge_->LoadRulesets(std::move(load_data),
                                      std::move(load_ruleset_callback));
}

void RulesMonitorService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK_EQ(context_, browser_context);

  // Return early if the extension does not have an active indexed ruleset.
  if (!extensions_with_rulesets_.erase(extension->id()))
    return;

  DCHECK(IsAPIAvailable());

  base::OnceClosure unload_ruleset_on_io_task =
      base::BindOnce(&UnloadRulesetOnIOThread, extension->id(),
                     base::RetainedRef(info_map_), context_);
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           std::move(unload_ruleset_on_io_task));
}

void RulesMonitorService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  DCHECK_EQ(context_, browser_context);

  // Skip if the extension will be reinstalled soon.
  if (reason == UNINSTALL_REASON_REINSTALL)
    return;

  // Skip if the extension doesn't have a dynamic ruleset.
  int dynamic_checksum;
  if (!prefs_->GetDNRDynamicRulesetChecksum(extension->id(), &dynamic_checksum))
    return;

  // Cleanup the dynamic rules directory for the extension.
  // TODO(karandeepb): It's possible that this task fails, e.g. during shutdown.
  // Make this more robust.
  RulesetSource source =
      RulesetSource::CreateDynamic(browser_context, *extension);
  DCHECK_EQ(source.json_path().DirName(), source.indexed_path().DirName());
  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                     source.json_path().DirName(), false /* recursive */));
}

void RulesMonitorService::OnRulesetLoaded(LoadRequestData load_data) {
  // Currently we only support a single static and an optional dynamic ruleset
  // per extension.
  DCHECK(load_data.rulesets.size() == 1u || load_data.rulesets.size() == 2u);
  RulesetInfo& static_ruleset = load_data.rulesets[0];
  DCHECK_EQ(static_ruleset.source().id(), RulesetSource::kStaticRulesetID)
      << static_ruleset.source().id();

  RulesetInfo* dynamic_ruleset =
      load_data.rulesets.size() == 2 ? &load_data.rulesets[1] : nullptr;
  DCHECK(!dynamic_ruleset ||
         dynamic_ruleset->source().id() == RulesetSource::kDynamicRulesetID)
      << dynamic_ruleset->source().id();

  // Update the ruleset checksums if needed.
  if (static_ruleset.new_checksum()) {
    prefs_->SetDNRRulesetChecksum(load_data.extension_id,
                                  *(static_ruleset.new_checksum()));
  }

  if (dynamic_ruleset && dynamic_ruleset->new_checksum()) {
    prefs_->SetDNRRulesetChecksum(load_data.extension_id,
                                  *(dynamic_ruleset->new_checksum()));
  }

  // It's possible that the extension has been disabled since the initial load
  // ruleset request. If it's disabled, do nothing.
  if (!extension_registry_->enabled_extensions().Contains(
          load_data.extension_id))
    return;

  CompositeMatcher::MatcherList matchers;
  if (static_ruleset.did_load_successfully()) {
    matchers.push_back(static_ruleset.TakeMatcher());
  }
  if (dynamic_ruleset && dynamic_ruleset->did_load_successfully()) {
    matchers.push_back(dynamic_ruleset->TakeMatcher());
  }

  // A ruleset failed to load. Notify the user.
  if (matchers.size() < load_data.rulesets.size()) {
    warning_service_->AddWarnings(
        {Warning::CreateRulesetFailedToLoadWarning(load_data.extension_id)});
  }

  if (matchers.empty())
    return;

  extensions_with_rulesets_.insert(load_data.extension_id);
  for (auto& observer : observers_)
    observer.OnRulesetLoaded();

  base::OnceClosure load_ruleset_on_io =
      base::BindOnce(&LoadRulesetOnIOThread, load_data.extension_id,
                     std::make_unique<CompositeMatcher>(std::move(matchers)),
                     prefs_->GetDNRAllowedPages(load_data.extension_id),
                     base::RetainedRef(info_map_), context_);
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           std::move(load_ruleset_on_io));
}

void RulesMonitorService::OnDynamicRulesUpdated(
    DynamicRuleUpdateUICallback callback,
    LoadRequestData load_data,
    base::Optional<std::string> error) {
  DCHECK_EQ(1u, load_data.rulesets.size());

  RulesetInfo& dynamic_ruleset = load_data.rulesets[0];
  DCHECK_EQ(dynamic_ruleset.did_load_successfully(), !error.has_value());

  // Update the ruleset checksums if needed. Note it's possible that
  // new_checksum() is valid while did_load_successfully() returns false below.
  // This should be rare but can happen when updating the rulesets succeeds but
  // we fail to create a RulesetMatcher from the indexed ruleset file (e.g. due
  // to a file read error). We still update the prefs checksum to ensure the
  // next ruleset load succeeds.
  if (dynamic_ruleset.new_checksum()) {
    prefs_->SetDNRDynamicRulesetChecksum(load_data.extension_id,
                                         *dynamic_ruleset.new_checksum());
  }

  // Respond to the extension.
  std::move(callback).Run(std::move(error));

  if (!dynamic_ruleset.did_load_successfully())
    return;

  DCHECK(dynamic_ruleset.new_checksum());

  // It's possible that the extension has been disabled since the initial update
  // rule request. If it's disabled, do nothing.
  if (!extension_registry_->enabled_extensions().Contains(
          load_data.extension_id)) {
    return;
  }

  // Update the dynamic ruleset on the IO thread.
  base::OnceClosure update_ruleset_on_io = base::BindOnce(
      &UpdateRulesetMatcherOnIOThread, load_data.extension_id,
      dynamic_ruleset.TakeMatcher(), base::RetainedRef(info_map_), context_);
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           std::move(update_ruleset_on_io));
}

}  // namespace declarative_net_request

template <>
void BrowserContextKeyedAPIFactory<
    declarative_net_request::RulesMonitorService>::
    DeclareFactoryDependencies() {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(WarningServiceFactory::GetInstance());
}

}  // namespace extensions
