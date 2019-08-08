// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_active_scenario.h"

#include <set>
#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom.h"

using base::trace_event::TraceConfig;
using Metrics = content::BackgroundTracingManagerImpl::Metrics;

namespace {
const size_t kDefaultTraceBufferSizeInKb = 10 * 1024;
}  // namespace

namespace content {

class BackgroundTracingActiveScenario::TracingTimer {
 public:
  TracingTimer(BackgroundTracingActiveScenario* scenario,
               BackgroundTracingManager::StartedFinalizingCallback callback)
      : scenario_(scenario), callback_(callback) {}
  ~TracingTimer() = default;

  void StartTimer(int seconds) {
    tracing_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(seconds), this,
                         &TracingTimer::TracingTimerFired);
  }
  void CancelTimer() { tracing_timer_.Stop(); }

  void FireTimerForTesting() {
    CancelTimer();
    TracingTimerFired();
  }

 private:
  void TracingTimerFired() { scenario_->BeginFinalizing(callback_); }

  BackgroundTracingActiveScenario* scenario_;
  base::OneShotTimer tracing_timer_;
  BackgroundTracingManager::StartedFinalizingCallback callback_;
};

class BackgroundTracingActiveScenario::TracingSession {
 public:
  virtual ~TracingSession() = default;
  virtual void BeginFinalizing(const base::RepeatingClosure& on_success,
                               const base::RepeatingClosure& on_failure) = 0;
  virtual void AbortScenario(
      const base::RepeatingClosure& on_abort_callback) = 0;
};

class PerfettoTracingSession
    : public BackgroundTracingActiveScenario::TracingSession,
      public tracing::mojom::TracingSessionClient,
      public mojo::DataPipeDrainer::Client {
 public:
  PerfettoTracingSession(BackgroundTracingActiveScenario* parent_scenario,
                         const TraceConfig& chrome_config,
                         BackgroundTracingConfigImpl::CategoryPreset preset)
      : parent_scenario_(parent_scenario),
        category_preset_(preset),
        raw_data_(std::make_unique<std::string>()) {
#if !defined(OS_ANDROID)
    // TODO(crbug.com/941318): Re-enable startup tracing for Android once all
    // Perfetto-related deadlocks are resolved.
    if (!TracingControllerImpl::GetInstance()->IsTracing() &&
        tracing::TracingUsesPerfettoBackend()) {
      tracing::TraceEventDataSource::GetInstance()->SetupStartupTracing(
          /*privacy_filtering_enabled=*/true);
    }
#endif

    ServiceManagerConnection::GetForProcess()->GetConnector()->BindInterface(
        tracing::mojom::kServiceName, &consumer_host_);

    perfetto::TraceConfig perfetto_config = tracing::GetDefaultPerfettoConfig(
        chrome_config, /*privacy_filtering_enabled=*/true);

    tracing::mojom::TracingSessionClientPtr tracing_session_client;
    binding_.Bind(mojo::MakeRequest(&tracing_session_client));
    binding_.set_connection_error_handler(
        base::BindOnce(&PerfettoTracingSession::OnTracingSessionEnded,
                       base::Unretained(this)));

    consumer_host_->EnableTracing(
        mojo::MakeRequest(&tracing_session_host_),
        std::move(tracing_session_client), std::move(perfetto_config),
        tracing::mojom::TracingClientPriority::kBackground);
    tracing_session_host_.set_connection_error_handler(
        base::BindOnce(&PerfettoTracingSession::OnTracingSessionEnded,
                       base::Unretained(this)));
  }

  // BackgroundTracingActiveScenario::TracingSession implementation.
  void BeginFinalizing(const base::RepeatingClosure& on_success,
                       const base::RepeatingClosure& on_failure) override {
    bool is_allowed_finalization =
        BackgroundTracingManagerImpl::GetInstance()->IsAllowedFinalization();

    if (!is_allowed_finalization) {
      on_failure.Run();
      return;
    }

    tracing_session_host_->DisableTracing();
    on_success.Run();
  }

  void AbortScenario(const base::RepeatingClosure& on_abort_callback) override {
    on_abort_callback.Run();
  }

  // mojo::DataPipeDrainer::Client implementation:
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    raw_data_->append(reinterpret_cast<const char*>(data), num_bytes);
  }

  void OnDataComplete() override {
    has_finished_receiving_data_ = true;
    MaybeFinishedReceivingTrace();
  }

  // tracing::mojom::TracingSession implementation:
  void OnTracingEnabled() override {
    BackgroundTracingManagerImpl::GetInstance()->OnStartTracingDone(
        category_preset_);
  }

  void OnTracingDisabled() override {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;

    MojoResult result =
        mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle);
    DCHECK_EQ(MOJO_RESULT_OK, result);

    drainer_ = std::make_unique<mojo::DataPipeDrainer>(
        this, std::move(consumer_handle));
    tracing_session_host_->ReadBuffers(
        std::move(producer_handle),
        base::BindOnce(&PerfettoTracingSession::OnReadBuffersComplete,
                       base::Unretained(this)));
  }

  void OnReadBuffersComplete() {
    has_finished_read_buffers_ = true;
    MaybeFinishedReceivingTrace();
  }

  void MaybeFinishedReceivingTrace() {
    if (has_finished_read_buffers_ && has_finished_receiving_data_) {
      DCHECK(raw_data_);
      parent_scenario_->OnProtoDataComplete(std::move(raw_data_));
    }
  }

 private:
  void OnTracingSessionEnded() { parent_scenario_->AbortScenario(); }

  BackgroundTracingActiveScenario* const parent_scenario_;
  mojo::Binding<tracing::mojom::TracingSessionClient> binding_{this};
  tracing::mojom::TracingSessionHostPtr tracing_session_host_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  tracing::mojom::ConsumerHostPtr consumer_host_;
  BackgroundTracingConfigImpl::CategoryPreset category_preset_;
  std::unique_ptr<std::string> raw_data_;
  bool has_finished_read_buffers_ = false;
  bool has_finished_receiving_data_ = false;
};

class LegacyTracingSession
    : public BackgroundTracingActiveScenario::TracingSession {
 public:
  LegacyTracingSession(BackgroundTracingActiveScenario* parent_scenario,
                       const TraceConfig& chrome_config,
                       BackgroundTracingConfigImpl::CategoryPreset preset)
      : parent_scenario_(parent_scenario) {
#if !defined(OS_ANDROID)
    // TODO(crbug.com/941318): Re-enable startup tracing for Android once all
    // Perfetto-related deadlocks are resolved.
    if (!TracingControllerImpl::GetInstance()->IsTracing() &&
        tracing::TracingUsesPerfettoBackend()) {
      tracing::TraceEventDataSource::GetInstance()->SetupStartupTracing(
          /*privacy_filtering_enabled=*/false);
    }
#endif

    bool result = TracingControllerImpl::GetInstance()->StartTracing(
        chrome_config,
        base::BindOnce(
            &BackgroundTracingManagerImpl::OnStartTracingDone,
            base::Unretained(BackgroundTracingManagerImpl::GetInstance()),
            preset));
    // We check IsEnabled() before creating the LegacyTracingSession,
    // so any failures to start tracing at this point would be due to invalid
    // configs which we treat as a failure scenario.
    DCHECK(result);
  }

  ~LegacyTracingSession() override {
    DCHECK(!TracingControllerImpl::GetInstance()->IsTracing());
  }

  // BackgroundTracingActiveScenario::TracingSession implementation.
  void BeginFinalizing(const base::RepeatingClosure& on_success,
                       const base::RepeatingClosure& on_failure) override {
    if (!BackgroundTracingManagerImpl::GetInstance()->IsAllowedFinalization()) {
      TracingControllerImpl::GetInstance()->StopTracing(
          TracingControllerImpl::CreateCallbackEndpoint(base::BindRepeating(
              [](const base::RepeatingClosure& on_failure,
                 std::unique_ptr<const base::DictionaryValue>,
                 base::RefCountedString*) { on_failure.Run(); },
              std::move(on_failure))));
      return;
    }

    auto trace_data_endpoint =
        TracingControllerImpl::CreateCompressedStringEndpoint(
            TracingControllerImpl::CreateCallbackEndpoint(base::BindRepeating(
                [](base::WeakPtr<BackgroundTracingActiveScenario> weak_this,
                   const base::RepeatingClosure& on_success,
                   std::unique_ptr<const base::DictionaryValue> metadata,
                   base::RefCountedString* file_contents) {
                  on_success.Run();
                  if (weak_this) {
                    weak_this->OnJSONDataComplete(std::move(metadata),
                                                  file_contents);
                  }
                },
                parent_scenario_->GetWeakPtr(), std::move(on_success))),
            true /* compress_with_background_priority */);

    TracingControllerImpl::GetInstance()->StopTracing(trace_data_endpoint);
  }

  void AbortScenario(const base::RepeatingClosure& on_abort_callback) override {
    if (TracingControllerImpl::GetInstance()->IsTracing()) {
      TracingControllerImpl::GetInstance()->StopTracing(
          TracingControllerImpl::CreateCallbackEndpoint(base::BindRepeating(
              [](const base::RepeatingClosure& on_abort_callback,
                 std::unique_ptr<const base::DictionaryValue>,
                 base::RefCountedString*) { on_abort_callback.Run(); },
              std::move(on_abort_callback))));
    } else {
      on_abort_callback.Run();
    }
  }

 private:
  BackgroundTracingActiveScenario* const parent_scenario_;
};

BackgroundTracingActiveScenario::BackgroundTracingActiveScenario(
    std::unique_ptr<BackgroundTracingConfigImpl> config,
    bool requires_anonymized_data,
    BackgroundTracingManager::ReceiveCallback receive_callback,
    base::OnceClosure on_aborted_callback)
    : config_(std::move(config)),
      requires_anonymized_data_(requires_anonymized_data),
      scenario_state_(State::kIdle),
      receive_callback_(std::move(receive_callback)),
      triggered_named_event_handle_(-1),
      on_aborted_callback_(std::move(on_aborted_callback)),
      weak_ptr_factory_(this) {
  DCHECK(config_ && !config_->rules().empty());
  for (const auto& rule : config_->rules()) {
    rule->Install();
  }
}

BackgroundTracingActiveScenario::~BackgroundTracingActiveScenario() = default;

const BackgroundTracingConfigImpl* BackgroundTracingActiveScenario::GetConfig()
    const {
  return config_.get();
}

void BackgroundTracingActiveScenario::SetState(State new_state) {
  auto old_state = scenario_state_;
  scenario_state_ = new_state;

  if ((old_state == State::kTracing) &&
      base::trace_event::TraceLog::GetInstance()->IsEnabled()) {
    // Leaving the kTracing state means we're supposed to have fully
    // shut down tracing at this point. Since StartTracing directly enables
    // tracing in TraceLog, in addition to going through Mojo, there's an
    // edge-case where tracing is rapidly stopped after starting, too quickly
    // for the TraceEventAgent of the browser process to register itself,
    // which means that we're left in a state where the Mojo interface doesn't
    // think we're tracing but TraceLog is still enabled. If that's the case,
    // we abort tracing here.
    auto record_mode =
        (config_->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE)
            ? base::trace_event::RECORD_CONTINUOUSLY
            : base::trace_event::RECORD_UNTIL_FULL;
    TraceConfig config =
        BackgroundTracingConfigImpl::GetConfigForCategoryPreset(
            config_->category_preset(), record_mode);

    uint8_t modes = base::trace_event::TraceLog::RECORDING_MODE;
    if (!config.event_filters().empty())
      modes |= base::trace_event::TraceLog::FILTERING_MODE;
    base::trace_event::TraceLog::GetInstance()->SetDisabled(modes);
  }

  if (scenario_state_ == State::kAborted) {
    tracing_session_.reset();
    std::move(on_aborted_callback_).Run();
  }
}

void BackgroundTracingActiveScenario::FireTimerForTesting() {
  DCHECK(tracing_timer_);
  tracing_timer_->FireTimerForTesting();
}

void BackgroundTracingActiveScenario::SetRuleTriggeredCallbackForTesting(
    const base::RepeatingClosure& callback) {
  rule_triggered_callback_for_testing_ = callback;
}

base::WeakPtr<BackgroundTracingActiveScenario>
BackgroundTracingActiveScenario::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BackgroundTracingActiveScenario::StartTracingIfConfigNeedsIt() {
  DCHECK(config_);
  if (config_->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE) {
    StartTracing(config_->category_preset(),
                 base::trace_event::RECORD_CONTINUOUSLY);
  }

  // There is nothing to do in case of reactive tracing.
}

bool BackgroundTracingActiveScenario::StartTracing(
    BackgroundTracingConfigImpl::CategoryPreset preset,
    base::trace_event::TraceRecordMode record_mode) {
  TraceConfig chrome_config =
      BackgroundTracingConfigImpl::GetConfigForCategoryPreset(preset,
                                                              record_mode);
  if (requires_anonymized_data_) {
    chrome_config.EnableArgumentFilter();
  }

  chrome_config.SetTraceBufferSizeInKb(kDefaultTraceBufferSizeInKb);

#if defined(OS_ANDROID)
  // Set low trace buffer size on Android in order to upload small trace files.
  if (config_->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE) {
    chrome_config.SetTraceBufferSizeInEvents(20000);
    chrome_config.SetTraceBufferSizeInKb(500);
  }
#endif

  // If the tracing controller is tracing, i.e. DevTools or about://tracing,
  // we don't start background tracing to not interfere with the user activity.
  if (TracingControllerImpl::GetInstance()->IsTracing()) {
    AbortScenario();
    return false;
  }

  // Activate the categories immediately. StartTracing eventually does this
  // itself, but asynchronously via Mojo, and in the meantime events will be
  // dropped. This ensures that we start recording events for those categories
  // immediately.
  uint8_t modes = base::trace_event::TraceLog::RECORDING_MODE;
  if (!chrome_config.event_filters().empty())
    modes |= base::trace_event::TraceLog::FILTERING_MODE;
  base::trace_event::TraceLog::GetInstance()->SetEnabled(chrome_config, modes);

  DCHECK(!tracing_session_);
  if (base::FeatureList::IsEnabled(features::kBackgroundTracingProtoOutput)) {
    tracing_session_ =
        std::make_unique<PerfettoTracingSession>(this, chrome_config, preset);
  } else {
    tracing_session_ =
        std::make_unique<LegacyTracingSession>(this, chrome_config, preset);
  }

  SetState(State::kTracing);
  BackgroundTracingManagerImpl::RecordMetric(Metrics::RECORDING_ENABLED);
  return true;
}

void BackgroundTracingActiveScenario::BeginFinalizing(
    BackgroundTracingManager::StartedFinalizingCallback callback) {
  triggered_named_event_handle_ = -1;
  tracing_timer_.reset();

  auto on_begin_finalization_success = base::BindRepeating(
      [](base::WeakPtr<BackgroundTracingActiveScenario> weak_this,
         BackgroundTracingManager::StartedFinalizingCallback callback) {
        if (!weak_this) {
          return;
        }

        weak_this->SetState(State::kFinalizing);
        BackgroundTracingManagerImpl::RecordMetric(
            Metrics::FINALIZATION_ALLOWED);
        DCHECK(!weak_this->started_finalizing_closure_);
        if (!callback.is_null()) {
          weak_this->started_finalizing_closure_ =
              base::BindOnce(callback, /*is_allowed_finalization=*/true);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), callback);

  auto on_begin_finalization_failure = base::BindRepeating(
      [](base::WeakPtr<BackgroundTracingActiveScenario> weak_this,
         BackgroundTracingManager::StartedFinalizingCallback callback) {
        if (!weak_this) {
          return;
        }

        BackgroundTracingManagerImpl::RecordMetric(
            Metrics::FINALIZATION_DISALLOWED);
        weak_this->SetState(State::kAborted);

        if (!callback.is_null()) {
          callback.Run(false);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), callback);

  tracing_session_->BeginFinalizing(std::move(on_begin_finalization_success),
                                    std::move(on_begin_finalization_failure));
}

void BackgroundTracingActiveScenario::OnJSONDataComplete(
    std::unique_ptr<const base::DictionaryValue> metadata,
    base::RefCountedString* file_contents) {
  BackgroundTracingManagerImpl::RecordMetric(Metrics::FINALIZATION_STARTED);
  UMA_HISTOGRAM_MEMORY_KB("Tracing.Background.FinalizingTraceSizeInKB",
                          file_contents->size() / 1024);

  // Send the finalized and compressed tracing data to the destination
  // callback.
  if (!receive_callback_.is_null()) {
    receive_callback_.Run(
        file_contents, std::move(metadata),
        base::BindOnce(&BackgroundTracingActiveScenario::OnFinalizeComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (started_finalizing_closure_) {
    std::move(started_finalizing_closure_).Run();
  }
}

void BackgroundTracingActiveScenario::OnProtoDataComplete(
    std::unique_ptr<std::string> proto_trace) {
  BackgroundTracingManagerImpl::RecordMetric(Metrics::FINALIZATION_STARTED);
  UMA_HISTOGRAM_MEMORY_KB("Tracing.Background.FinalizingTraceSizeInKB",
                          proto_trace->size() / 1024);

  BackgroundTracingManagerImpl::GetInstance()->SetTraceToUpload(
      std::move(proto_trace));

  if (started_finalizing_closure_) {
    std::move(started_finalizing_closure_).Run();
  }
}

void BackgroundTracingActiveScenario::OnFinalizeComplete(bool success) {
  if (success) {
    BackgroundTracingManagerImpl::RecordMetric(Metrics::UPLOAD_SUCCEEDED);
  } else {
    BackgroundTracingManagerImpl::RecordMetric(Metrics::UPLOAD_FAILED);
  }

  tracing_session_.reset();
  SetState(State::kIdle);

  // Now that a trace has completed, we may need to enable recording again.
  StartTracingIfConfigNeedsIt();
}

void BackgroundTracingActiveScenario::AbortScenario() {
  if (tracing_session_) {
    tracing_session_->AbortScenario(base::BindRepeating(
        [](base::WeakPtr<BackgroundTracingActiveScenario> weak_this) {
          if (weak_this) {
            weak_this->SetState(State::kAborted);
          }
        },
        weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Setting the kAborted state will cause |this| to be destroyed.
    SetState(State::kAborted);
  }
}

void BackgroundTracingActiveScenario::TriggerNamedEvent(
    BackgroundTracingManager::TriggerHandle handle,
    BackgroundTracingManager::StartedFinalizingCallback callback) {
  std::string trigger_name =
      BackgroundTracingManagerImpl::GetInstance()->GetTriggerNameFromHandle(
          handle);
  auto* triggered_rule = GetRuleAbleToTriggerTracing(trigger_name);
  if (!triggered_rule) {
    if (!callback.is_null()) {
      std::move(callback).Run(false);
    }
    return;
  }

  // A different reactive config than the running one tried to trigger.
  if ((config_->tracing_mode() == BackgroundTracingConfigImpl::REACTIVE &&
       (state() == State::kTracing) &&
       triggered_named_event_handle_ != handle)) {
    if (!callback.is_null()) {
      std::move(callback).Run(false);
    }
    return;
  }

  triggered_named_event_handle_ = handle;
  OnRuleTriggered(triggered_rule, std::move(callback));
}

void BackgroundTracingActiveScenario::OnHistogramTrigger(
    const std::string& histogram_name) {
  for (const auto& rule : config_->rules()) {
    if (rule->ShouldTriggerNamedEvent(histogram_name)) {
      OnRuleTriggered(rule.get(),
                      BackgroundTracingManager::StartedFinalizingCallback());
    }
  }
}

void BackgroundTracingActiveScenario::OnRuleTriggered(
    const BackgroundTracingRule* triggered_rule,
    BackgroundTracingManager::StartedFinalizingCallback callback) {
  DCHECK_NE(state(), State::kAborted);
  double trigger_chance = triggered_rule->trigger_chance();
  if (trigger_chance < 1.0 && base::RandDouble() > trigger_chance) {
    if (!callback.is_null()) {
      std::move(callback).Run(false);
    }
    return;
  }

  last_triggered_rule_.reset(new base::DictionaryValue);
  triggered_rule->IntoDict(last_triggered_rule_.get());

  int trace_delay = triggered_rule->GetTraceDelay();

  if (config_->tracing_mode() == BackgroundTracingConfigImpl::REACTIVE) {
    // In reactive mode, a trigger starts tracing, or finalizes tracing
    // immediately if it's already running.
    BackgroundTracingManagerImpl::RecordMetric(Metrics::REACTIVE_TRIGGERED);

    if (state() != State::kTracing) {
      // It was not already tracing, start a new trace.
      if (!StartTracing(triggered_rule->category_preset(),
                        base::trace_event::RECORD_UNTIL_FULL)) {
        return;
      }
    } else {
      // Some reactive configs that trigger again while tracing should just
      // end right away (to not capture multiple navigations, for example).
      // For others we just want to ignore the repeated trigger.
      if (triggered_rule->stop_tracing_on_repeated_reactive()) {
        trace_delay = -1;
      } else {
        if (!callback.is_null()) {
          std::move(callback).Run(false);
        }
        return;
      }
    }
  } else {
    // In preemptive mode, a trigger starts finalizing a trace if one is
    // running and we haven't got a finalization timer running,
    // otherwise we do nothing.
    if ((state() != State::kTracing) || tracing_timer_) {
      if (!callback.is_null()) {
        std::move(callback).Run(false);
      }
      return;
    }

    BackgroundTracingManagerImpl::RecordMetric(Metrics::PREEMPTIVE_TRIGGERED);
  }

  if (trace_delay < 0) {
    BeginFinalizing(std::move(callback));
  } else {
    tracing_timer_ = std::make_unique<TracingTimer>(this, std::move(callback));
    tracing_timer_->StartTimer(trace_delay);
  }

  if (!rule_triggered_callback_for_testing_.is_null()) {
    rule_triggered_callback_for_testing_.Run();
  }
}

BackgroundTracingRule*
BackgroundTracingActiveScenario::GetRuleAbleToTriggerTracing(
    const std::string& trigger_name) {
  // If the last trace is still uploading, we don't allow a new one to trigger.
  if (state() == State::kFinalizing) {
    return nullptr;
  }

  for (const auto& rule : config_->rules()) {
    if (rule->ShouldTriggerNamedEvent(trigger_name)) {
      return rule.get();
    }
  }

  return nullptr;
}

void BackgroundTracingActiveScenario::GenerateMetadataDict(
    base::DictionaryValue* metadata_dict) {
  auto config_dict = std::make_unique<base::DictionaryValue>();
  config_->IntoDict(config_dict.get());
  metadata_dict->Set("config", std::move(config_dict));
  metadata_dict->SetString("scenario_name", config_->scenario_name());

  if (last_triggered_rule_) {
    metadata_dict->Set("last_triggered_rule", std::move(last_triggered_rule_));
  }
}

}  // namespace content
