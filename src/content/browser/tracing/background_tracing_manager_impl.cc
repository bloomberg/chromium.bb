// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/tracing/common/trace_startup_config.h"
#include "content/browser/tracing/background_memory_tracing_observer.h"
#include "content/browser/tracing/background_startup_tracing_observer.h"
#include "content/browser/tracing/background_tracing_active_scenario.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/cpp/tracing_features.h"

using base::trace_event::TraceConfig;

namespace content {

// static
void BackgroundTracingManagerImpl::RecordMetric(Metrics metric) {
  UMA_HISTOGRAM_ENUMERATION("Tracing.Background.ScenarioState", metric,
                            Metrics::NUMBER_OF_BACKGROUND_TRACING_METRICS);
}

// static
BackgroundTracingManager* BackgroundTracingManager::GetInstance() {
  return BackgroundTracingManagerImpl::GetInstance();
}

// static
BackgroundTracingManagerImpl* BackgroundTracingManagerImpl::GetInstance() {
  static base::NoDestructor<BackgroundTracingManagerImpl> manager;
  return manager.get();
}

BackgroundTracingManagerImpl::BackgroundTracingManagerImpl()
    : delegate_(GetContentClient()->browser()->GetTracingDelegate()),
      trigger_handle_ids_(0) {
  AddEnabledStateObserver(BackgroundMemoryTracingObserver::GetInstance());
  AddEnabledStateObserver(BackgroundStartupTracingObserver::GetInstance());
}

BackgroundTracingManagerImpl::~BackgroundTracingManagerImpl() = default;

void BackgroundTracingManagerImpl::AddMetadataGeneratorFunction() {
  tracing::TraceEventAgent::GetInstance()->AddMetadataGeneratorFunction(
      base::BindRepeating(&BackgroundTracingManagerImpl::GenerateMetadataDict,
                          base::Unretained(this)));
}

bool BackgroundTracingManagerImpl::SetActiveScenario(
    std::unique_ptr<BackgroundTracingConfig> config,
    BackgroundTracingManager::ReceiveCallback receive_callback,
    DataFiltering data_filtering) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (config) {
    RecordMetric(Metrics::SCENARIO_ACTIVATION_REQUESTED);
  }

  if (active_scenario_ && (active_scenario_->state() !=
                           BackgroundTracingActiveScenario::State::kIdle)) {
    return false;
  }

  // If we don't have a high resolution timer available, traces will be
  // too inaccurate to be useful.
  if (!base::TimeTicks::IsHighResolution()) {
    if (config) {
      RecordMetric(Metrics::SCENARIO_ACTION_FAILED_LOWRES_CLOCK);
    }
    return false;
  }

  std::unique_ptr<BackgroundTracingConfigImpl> config_impl(
      static_cast<BackgroundTracingConfigImpl*>(config.release()));
  config_impl = BackgroundStartupTracingObserver::GetInstance()
                    ->IncludeStartupConfigIfNeeded(std::move(config_impl));
  if (BackgroundStartupTracingObserver::GetInstance()
          ->enabled_in_current_session()) {
    // Anonymize data for startup tracing by default. We currently do not
    // support storing the config in preferences for next session.
    data_filtering = DataFiltering::ANONYMIZE_DATA;
    RecordMetric(Metrics::STARTUP_SCENARIO_TRIGGERED);
  } else {
    // If startup config was not set and tracing was enabled, then do not set
    // any scenario.
    if (base::trace_event::TraceLog::GetInstance()->IsEnabled()) {
      return false;
    }
  }

  if (!config_impl) {
    return false;
  }

  bool requires_anonymized_data = (data_filtering == ANONYMIZE_DATA);

  // If the profile hasn't loaded or been created yet, this is a startup
  // scenario and we have to wait until initialization is finished to validate
  // that the scenario can run.
  if (!delegate_ || delegate_->IsProfileLoaded()) {
    // TODO(oysteine): Retry when time_until_allowed has elapsed.
    if (config_impl && delegate_ &&
        !delegate_->IsAllowedToBeginBackgroundScenario(
            *config_impl.get(), requires_anonymized_data)) {
      return false;
    }
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&BackgroundTracingManagerImpl::ValidateStartupScenario,
                       base::Unretained(this)));
  }

  // No point in tracing if there's nowhere to send it.
  if (config_impl && receive_callback.is_null()) {
    return false;
  }

  active_scenario_ = std::make_unique<BackgroundTracingActiveScenario>(
      std::move(config_impl), requires_anonymized_data,
      std::move(receive_callback),
      base::BindOnce(&BackgroundTracingManagerImpl::OnScenarioAborted,
                     base::Unretained(this)));

  // Notify observers before starting tracing.
  for (auto* observer : background_tracing_observers_) {
    observer->OnScenarioActivated(active_scenario_->GetConfig());
  }

  active_scenario_->StartTracingIfConfigNeedsIt();
  RecordMetric(Metrics::SCENARIO_ACTIVATED_SUCCESSFULLY);

  return true;
}

bool BackgroundTracingManagerImpl::HasActiveScenario() {
  return !!active_scenario_;
}

bool BackgroundTracingManagerImpl::HasTraceToUpload() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return !trace_to_upload_.empty();
}

std::string BackgroundTracingManagerImpl::GetLatestTraceToUpload() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string ret;
  ret.swap(trace_to_upload_);

  if (active_scenario_) {
    active_scenario_->OnFinalizeComplete(true);
  }

  return ret;
}

void BackgroundTracingManagerImpl::AddEnabledStateObserver(
    EnabledStateObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  background_tracing_observers_.insert(observer);
}

void BackgroundTracingManagerImpl::RemoveEnabledStateObserver(
    EnabledStateObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  background_tracing_observers_.erase(observer);
}

void BackgroundTracingManagerImpl::AddTraceMessageFilter(
    TraceMessageFilter* trace_message_filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  trace_message_filters_.insert(trace_message_filter);

  for (auto* observer : trace_message_filter_observers_) {
    observer->OnTraceMessageFilterAdded(trace_message_filter);
  }
}

void BackgroundTracingManagerImpl::RemoveTraceMessageFilter(
    TraceMessageFilter* trace_message_filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto* observer : trace_message_filter_observers_) {
    observer->OnTraceMessageFilterRemoved(trace_message_filter);
  }

  trace_message_filters_.erase(trace_message_filter);
}

void BackgroundTracingManagerImpl::AddTraceMessageFilterObserver(
    TraceMessageFilterObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  trace_message_filter_observers_.insert(observer);

  for (auto& filter : trace_message_filters_) {
    observer->OnTraceMessageFilterAdded(filter.get());
  }
}

void BackgroundTracingManagerImpl::RemoveTraceMessageFilterObserver(
    TraceMessageFilterObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  trace_message_filter_observers_.erase(observer);

  for (auto& filter : trace_message_filters_) {
    observer->OnTraceMessageFilterRemoved(filter.get());
  }
}

BackgroundTracingActiveScenario*
BackgroundTracingManagerImpl::GetActiveScenarioForTesting() {
  DCHECK(active_scenario_);
  return active_scenario_.get();
}

bool BackgroundTracingManagerImpl::IsTracingForTesting() {
  return active_scenario_ && (active_scenario_->state() ==
                              BackgroundTracingActiveScenario::State::kTracing);
}

void BackgroundTracingManagerImpl::SetTraceToUploadForTesting(
    std::unique_ptr<std::string> trace_data) {
  SetTraceToUpload(std::move(trace_data));
}

void BackgroundTracingManagerImpl::SetTraceToUpload(
    std::unique_ptr<std::string> trace_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (trace_data) {
    trace_to_upload_.swap(*trace_data);
  } else {
    trace_to_upload_.clear();
  }
}

void BackgroundTracingManagerImpl::ValidateStartupScenario() {
  if (!active_scenario_ || !delegate_) {
    return;
  }

  if (!delegate_->IsAllowedToBeginBackgroundScenario(
          *active_scenario_->GetConfig(),
          active_scenario_->requires_anonymized_data())) {
    AbortScenario();
  }
}


void BackgroundTracingManagerImpl::OnHistogramTrigger(
    const std::string& histogram_name) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&BackgroundTracingManagerImpl::OnHistogramTrigger,
                       base::Unretained(this), histogram_name));
    return;
  }

  if (active_scenario_) {
    active_scenario_->OnHistogramTrigger(histogram_name);
  }
}

void BackgroundTracingManagerImpl::TriggerNamedEvent(
    BackgroundTracingManagerImpl::TriggerHandle handle,
    StartedFinalizingCallback callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&BackgroundTracingManagerImpl::TriggerNamedEvent,
                       base::Unretained(this), handle, std::move(callback)));
    return;
  }

  if (!active_scenario_ || !IsTriggerHandleValid(handle)) {
    if (!callback.is_null()) {
      std::move(callback).Run(false);
    }
    return;
  }

  active_scenario_->TriggerNamedEvent(handle, std::move(callback));
}

void BackgroundTracingManagerImpl::OnRuleTriggered(
    const BackgroundTracingRule* triggered_rule,
    StartedFinalizingCallback callback) {
  // The active scenario can be null here if scenario was aborted during
  // validation and the rule was triggered just before validation. If validation
  // kicked in after this point, we still check before uploading.
  if (active_scenario_) {
    active_scenario_->OnRuleTriggered(triggered_rule, callback);
  }
}

BackgroundTracingManagerImpl::TriggerHandle
BackgroundTracingManagerImpl::RegisterTriggerType(const char* trigger_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  trigger_handle_ids_ += 1;
  trigger_handles_.insert(
      std::pair<TriggerHandle, std::string>(trigger_handle_ids_, trigger_name));

  return static_cast<TriggerHandle>(trigger_handle_ids_);
}

bool BackgroundTracingManagerImpl::IsTriggerHandleValid(
    BackgroundTracingManager::TriggerHandle handle) const {
  return trigger_handles_.find(handle) != trigger_handles_.end();
}

std::string BackgroundTracingManagerImpl::GetTriggerNameFromHandle(
    BackgroundTracingManager::TriggerHandle handle) const {
  CHECK(IsTriggerHandleValid(handle));
  return trigger_handles_.find(handle)->second;
}

void BackgroundTracingManagerImpl::InvalidateTriggerHandlesForTesting() {
  trigger_handles_.clear();
}

void BackgroundTracingManagerImpl::OnStartTracingDone(
    BackgroundTracingConfigImpl::CategoryPreset preset) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto* observer : background_tracing_observers_) {
    observer->OnTracingEnabled(preset);
  }
}

void BackgroundTracingManagerImpl::WhenIdle(
    base::RepeatingClosure idle_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  idle_callback_ = std::move(idle_callback);

  if (!active_scenario_) {
    idle_callback_.Run();
  }
}

bool BackgroundTracingManagerImpl::IsAllowedFinalization() const {
  return !delegate_ || (active_scenario_ &&
                        delegate_->IsAllowedToEndBackgroundScenario(
                            *active_scenario_->GetConfig(),
                            active_scenario_->requires_anonymized_data()));
}

std::unique_ptr<base::DictionaryValue>
BackgroundTracingManagerImpl::GenerateMetadataDict() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto metadata_dict = std::make_unique<base::DictionaryValue>();
  if (active_scenario_) {
    active_scenario_->GenerateMetadataDict(metadata_dict.get());
  }

  return metadata_dict;
}

void BackgroundTracingManagerImpl::AbortScenarioForTesting() {
  AbortScenario();
}

void BackgroundTracingManagerImpl::AbortScenario() {
  if (active_scenario_) {
    active_scenario_->AbortScenario();
  }
}

void BackgroundTracingManagerImpl::OnScenarioAborted() {
  DCHECK(active_scenario_);

  // Don't synchronously delete to avoid use-after-free issues in
  // BackgroundTracingActiveScenario.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                  std::move(active_scenario_));

  for (auto* observer : background_tracing_observers_) {
    observer->OnScenarioAborted();
  }

  if (!idle_callback_.is_null()) {
    idle_callback_.Run();
  }
}

}  // namespace content
