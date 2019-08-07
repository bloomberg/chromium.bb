// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULES_MONITOR_SERVICE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULES_MONITOR_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class InfoMap;
class ExtensionPrefs;
class ExtensionRegistry;
class WarningService;

namespace api {
namespace declarative_net_request {
struct Rule;
}  // namespace declarative_net_request
}  // namespace api

namespace declarative_net_request {
enum class DynamicRuleUpdateAction;
struct LoadRequestData;

// Observes loading and unloading of extensions to load and unload their
// rulesets for the Declarative Net Request API. Lives on the UI thread. Note: A
// separate instance of RulesMonitorService is not created for incognito. Both
// the incognito and normal contexts will share the same ruleset.
class RulesMonitorService : public BrowserContextKeyedAPI,
                            public ExtensionRegistryObserver {
 public:
  class Observer {
   public:
    // Called when this service loads a new ruleset.
    virtual void OnRulesetLoaded() = 0;

   protected:
    virtual ~Observer() {}
  };

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<RulesMonitorService>*
  GetFactoryInstance();

  bool HasAnyRegisteredRulesets() const;

  // Returns true if there is registered declarative ruleset corresponding to
  // the given |extension_id|.
  bool HasRegisteredRuleset(const ExtensionId& extension_id) const;

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Updates the dynamic rules for the |extension| and then invokes
  // |callback| with an optional error.
  using DynamicRuleUpdateUICallback =
      base::OnceCallback<void(base::Optional<std::string> error)>;
  void UpdateDynamicRules(const Extension& extension,
                          std::vector<api::declarative_net_request::Rule> rules,
                          DynamicRuleUpdateAction action,
                          DynamicRuleUpdateUICallback callback);

 private:
  class FileSequenceBridge;

  friend class BrowserContextKeyedAPIFactory<RulesMonitorService>;

  // The constructor is kept private since this should only be created by the
  // BrowserContextKeyedAPIFactory.
  explicit RulesMonitorService(content::BrowserContext* browser_context);

  ~RulesMonitorService() override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "RulesMonitorService"; }
  static const bool kServiceIsNULLWhileTesting = true;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

  // Invoked when we have loaded the ruleset on |file_task_runner_|.
  void OnRulesetLoaded(LoadRequestData load_data);

  // Invoked when the dynamic rules for the extension have been updated.
  void OnDynamicRulesUpdated(DynamicRuleUpdateUICallback callback,
                             LoadRequestData load_data,
                             base::Optional<std::string> error);

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observer_;

  std::set<ExtensionId> extensions_with_rulesets_;

  // Helper to bridge tasks to a sequence which allows file IO.
  std::unique_ptr<const FileSequenceBridge> file_sequence_bridge_;

  // Guaranteed to be valid through-out the lifetime of this instance.
  InfoMap* const info_map_;
  ExtensionPrefs* const prefs_;
  ExtensionRegistry* const extension_registry_;
  WarningService* const warning_service_;

  base::ObserverList<Observer>::Unchecked observers_;

  content::BrowserContext* const context_;

  // Must be the last member variable. See WeakPtrFactory documentation for
  // details.
  base::WeakPtrFactory<RulesMonitorService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RulesMonitorService);
};

}  // namespace declarative_net_request

template <>
void BrowserContextKeyedAPIFactory<
    declarative_net_request::RulesMonitorService>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULES_MONITOR_SERVICE_H_
