// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_reached_code_tracing_observer_android.h"

#include "base/android/reached_code_profiler.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

bool EnableReachedCodeProfiler() {
  if (!base::android::IsReachedCodeProfilerSupported())
    return false;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableReachedCodeProfiler);
  base::android::InitReachedCodeProfilerAtStartup(
      base::android::PROCESS_BROWSER);
  EXPECT_TRUE(base::android::IsReachedCodeProfilerEnabled);
  return true;
}

const BackgroundTracingRule* FindReachedCodeRuleInConfig(
    const BackgroundTracingConfigImpl& config) {
  for (const auto& rule : config.rules()) {
    if (rule->rule_id().find("reached-code-config") != std::string::npos) {
      return rule.get();
    }
  }
  return nullptr;
}

std::unique_ptr<BackgroundTracingConfigImpl> GetGpuConfig() {
  auto rules_dict = std::make_unique<base::DictionaryValue>();
  rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
  rules_dict->SetString("trigger_name", "test");
  rules_dict->SetString("category", "BENCHMARK_GPU");
  base::DictionaryValue dict;
  auto rules_list = std::make_unique<base::ListValue>();
  rules_list->Append(std::move(rules_dict));
  dict.Set("configs", std::move(rules_list));
  return BackgroundTracingConfigImpl::ReactiveFromDict(&dict);
}

std::unique_ptr<BackgroundTracingConfigImpl> GetReachedCodeConfig() {
  auto rules_dict = std::make_unique<base::DictionaryValue>();
  rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
  rules_dict->SetString("trigger_name", "reached-code-config");
  rules_dict->SetInteger("trigger_delay", 30);

  base::DictionaryValue dict;
  auto rules_list = std::make_unique<base::ListValue>();
  rules_list->Append(std::move(rules_dict));
  dict.Set("configs", std::move(rules_list));
  dict.SetString(
      "enabled_data_sources",
      "org.chromium.trace_metadata,org.chromium.reached_code_profiler");
  dict.SetString("category", "CUSTOM");
  dict.SetString("custom_categories", "-*");
  auto config = BackgroundTracingConfigImpl::ReactiveFromDict(&dict);
  EXPECT_TRUE(FindReachedCodeRuleInConfig(*config));
  return config;
}

void TestReachedCodeRuleExists(const BackgroundTracingConfigImpl& config,
                               bool exists) {
  const auto* rule = FindReachedCodeRuleInConfig(config);
  if (exists) {
    ASSERT_TRUE(rule);
    EXPECT_EQ(30, rule->GetTraceDelay());
    EXPECT_FALSE(rule->stop_tracing_on_repeated_reactive());
    EXPECT_EQ("org.chromium.trace_metadata,org.chromium.reached_code_profiler",
              config.enabled_data_sources());
  } else {
    EXPECT_FALSE(rule);
  }
}

void TestGpuConfigExists(const BackgroundTracingConfigImpl& config) {
  bool found_gpu = false;
  for (const auto& rule : config.rules()) {
    if (rule->category_preset() ==
        BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_GPU) {
      found_gpu = true;
    }
  }
  EXPECT_TRUE(found_gpu);
}

}  // namespace

TEST(BackgroundReachedCodeTracingObserverTest,
     IncludeReachedCodeConfigIfNeeded) {
  EXPECT_FALSE(base::android::IsReachedCodeProfilerEnabled());
  BackgroundReachedCodeTracingObserver& observer =
      BackgroundReachedCodeTracingObserver::GetInstance();

  // Empty config without profiler set should not do anything.
  std::unique_ptr<content::BackgroundTracingConfigImpl> config_impl;
  config_impl = observer.IncludeReachedCodeConfigIfNeeded(nullptr);
  EXPECT_FALSE(config_impl);
  EXPECT_FALSE(observer.enabled_in_current_session());
  EXPECT_FALSE(base::android::IsReachedCodeProfilerEnabled());

  // A GPU config without preference set should not set preference and keep
  // config same.
  config_impl = GetGpuConfig();
  ASSERT_TRUE(config_impl);

  EXPECT_FALSE(base::android::IsReachedCodeProfilerEnabled());
  config_impl =
      observer.IncludeReachedCodeConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(observer.enabled_in_current_session());
  EXPECT_FALSE(base::android::IsReachedCodeProfilerEnabled());
  EXPECT_EQ(1u, config_impl->rules().size());
  TestReachedCodeRuleExists(*config_impl, false);
  TestGpuConfigExists(*config_impl);

  // A reached code config without profiler should stay config same.
  config_impl = GetReachedCodeConfig();
  config_impl =
      observer.IncludeReachedCodeConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(observer.enabled_in_current_session());
  ASSERT_TRUE(config_impl);
  EXPECT_FALSE(base::android::IsReachedCodeProfilerEnabled());
  EXPECT_EQ(1u, config_impl->rules().size());
  TestReachedCodeRuleExists(*config_impl, true);

  if (!base::android::IsReachedCodeProfilerSupported())
    return;
  config_impl.reset();
  EXPECT_TRUE(EnableReachedCodeProfiler());
  BackgroundReachedCodeTracingObserver::ResetForTesting();
  EXPECT_TRUE(observer.enabled_in_current_session());

  // Empty config with profiler should create a config.
  EXPECT_TRUE(base::android::IsReachedCodeProfilerEnabled());
  config_impl =
      observer.IncludeReachedCodeConfigIfNeeded(std::move(config_impl));
  EXPECT_TRUE(base::android::IsReachedCodeProfilerEnabled());
  EXPECT_TRUE(observer.enabled_in_current_session());
  ASSERT_TRUE(config_impl);
  EXPECT_EQ(1u, config_impl->rules().size());
  EXPECT_EQ(BackgroundTracingConfig::TracingMode::REACTIVE,
            config_impl->tracing_mode());
  TestReachedCodeRuleExists(*config_impl, true);

  // A GPU config with profiler on should not enabled reached code config.
  config_impl = GetGpuConfig();
  config_impl =
      observer.IncludeReachedCodeConfigIfNeeded(std::move(config_impl));
  EXPECT_FALSE(observer.enabled_in_current_session());
  ASSERT_TRUE(config_impl);
  EXPECT_TRUE(base::android::IsReachedCodeProfilerEnabled());
  EXPECT_EQ(1u, config_impl->rules().size());
  EXPECT_EQ(BackgroundTracingConfig::TracingMode::REACTIVE,
            config_impl->tracing_mode());
  TestReachedCodeRuleExists(*config_impl, false);

  TestGpuConfigExists(*config_impl);
}

}  // namespace content
