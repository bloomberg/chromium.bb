// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_startup_tracing_observer.h"

#include "base/bind.h"
#include "components/tracing/common/trace_startup_config.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

const char kStartupTracingConfig[] = "startup-config";

class PreferenceManagerImpl
    : public BackgroundStartupTracingObserver::PreferenceManager {
 public:
  void SetBackgroundStartupTracingEnabled(bool enabled) override {
    tracing::TraceStartupConfig::GetInstance()
        ->SetBackgroundStartupTracingEnabled(enabled);
  }

  bool GetBackgroundStartupTracingEnabled() const override {
    return tracing::TraceStartupConfig::GetInstance()->IsEnabled() &&
           tracing::TraceStartupConfig::GetInstance()->GetSessionOwner() ==
               tracing::TraceStartupConfig::SessionOwner::kBackgroundTracing;
  }
};

}  // namespace

// static
BackgroundStartupTracingObserver*
BackgroundStartupTracingObserver::GetInstance() {
  static BackgroundStartupTracingObserver* instance =
      new BackgroundStartupTracingObserver;
  return instance;
}

// static
const BackgroundTracingRule*
BackgroundStartupTracingObserver::FindStartupRuleInConfig(
    const BackgroundTracingConfigImpl& config) {
  for (const auto& rule : config.rules()) {
    if (rule->category_preset() ==
        BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_STARTUP) {
      return rule.get();
    }
  }
  return nullptr;
}

BackgroundStartupTracingObserver::BackgroundStartupTracingObserver()
    : enabled_in_current_session_(false),
      preferences_(new PreferenceManagerImpl) {}

BackgroundStartupTracingObserver::~BackgroundStartupTracingObserver() {}

void BackgroundStartupTracingObserver::OnScenarioActivated(
    const BackgroundTracingConfigImpl* config) {
  if (!enabled_in_current_session_)
    return;
  const BackgroundTracingRule* startup_rule = FindStartupRuleInConfig(*config);
  DCHECK(startup_rule);

  // Post task to avoid reentrancy.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BackgroundTracingManagerImpl::OnRuleTriggered,
          base::Unretained(BackgroundTracingManagerImpl::GetInstance()),
          base::Unretained(startup_rule),
          BackgroundTracingManager::StartedFinalizingCallback()));
}

void BackgroundStartupTracingObserver::OnScenarioAborted() {
  enabled_in_current_session_ = false;
}

void BackgroundStartupTracingObserver::OnTracingEnabled(
    BackgroundTracingConfigImpl::CategoryPreset preset) {}

void BackgroundStartupTracingObserver::SetPreferenceManagerForTesting(
    std::unique_ptr<PreferenceManager> preferences) {
  preferences_ = std::move(preferences);
}

std::unique_ptr<BackgroundTracingConfigImpl>
BackgroundStartupTracingObserver::IncludeStartupConfigIfNeeded(
    std::unique_ptr<BackgroundTracingConfigImpl> config) {
  enabled_in_current_session_ =
      preferences_->GetBackgroundStartupTracingEnabled();

  const BackgroundTracingRule* startup_rule = nullptr;
  if (config) {
    startup_rule = FindStartupRuleInConfig(*config);
  }

  // Reset the flag if startup tracing was enabled again in current session.
  if (startup_rule) {
    preferences_->SetBackgroundStartupTracingEnabled(true);
  } else {
    preferences_->SetBackgroundStartupTracingEnabled(false);
  }

  // If we're preemptive tracing then OnScenarioActivated() would just
  // immediately finalize tracing, rather than starting it.
  if (config &&
      (config->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE)) {
    enabled_in_current_session_ = false;
    return config;
  }

  // If enabled in current session and startup rule already exists, then do not
  // add another rule.
  if (!enabled_in_current_session_ || startup_rule)
    return config;

  base::Value rules_dict(base::Value::Type::DICTIONARY);
  rules_dict.SetStringKey("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
  rules_dict.SetStringKey("trigger_name", kStartupTracingConfig);
  rules_dict.SetIntKey("trigger_delay", 30);
  rules_dict.SetStringKey("category", "BENCHMARK_STARTUP");

  if (config) {
    config->AddReactiveRule(
        rules_dict,
        BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_STARTUP);
  } else {
    base::Value dict(base::Value::Type::DICTIONARY);
    base::Value rules_list(base::Value::Type::LIST);
    rules_list.Append(std::move(rules_dict));
    dict.SetKey("configs", std::move(rules_list));
    config = BackgroundTracingConfigImpl::ReactiveFromDict(dict);
  }
  DCHECK(FindStartupRuleInConfig(*config));
  return config;
}

}  // namespace content
