// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/attribution_simulator.h"

#include <stddef.h>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_default_random_generator.h"
#include "content/browser/attribution_reporting/attribution_insecure_random_generator.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_random_generator.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_report_sender.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/attribution_simulator_input_parser.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace content {

namespace {

base::Time GetEventTime(const AttributionSimulationEventAndValue& event) {
  struct Visitor {
    base::Time operator()(const StorableSource& source) {
      return source.common_info().impression_time();
    }

    base::Time operator()(const AttributionTriggerAndTime& trigger) {
      return trigger.time;
    }
  };

  return absl::visit(Visitor{}, event.first);
}

// TODO(apaseltiner): Consider exposing other behaviors here.
class AlwaysSetCookieChecker : public AttributionCookieChecker {
 public:
  AlwaysSetCookieChecker() = default;

  ~AlwaysSetCookieChecker() override = default;

  AlwaysSetCookieChecker(const AlwaysSetCookieChecker&) = delete;
  AlwaysSetCookieChecker(AlwaysSetCookieChecker&&) = delete;

  AlwaysSetCookieChecker& operator=(const AlwaysSetCookieChecker&) = delete;
  AlwaysSetCookieChecker& operator=(AlwaysSetCookieChecker&&) = delete;

 private:
  // AttributionManagerImpl::CookieChecker:
  void IsDebugCookieSet(const url::Origin& origin,
                        base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(true);
  }
};

class SentReportAccumulator : public AttributionReportSender {
 public:
  SentReportAccumulator(base::Value::ListStorage& event_level_reports,
                        base::Value::ListStorage& debug_event_level_reports,
                        base::Value::ListStorage& aggregatable_reports,
                        base::Value::ListStorage& debug_aggregatable_reports,
                        bool remove_report_ids,
                        AttributionReportTimeFormat report_time_format,
                        bool remove_assembled_report)
      : time_origin_(base::Time::Now()),
        remove_report_ids_(remove_report_ids),
        report_time_format_(report_time_format),
        remove_assembled_report_(remove_assembled_report),
        event_level_reports_(event_level_reports),
        debug_event_level_reports_(debug_event_level_reports),
        aggregatable_reports_(aggregatable_reports),
        debug_aggregatable_reports_(debug_aggregatable_reports) {}

  ~SentReportAccumulator() override = default;

  SentReportAccumulator(const SentReportAccumulator&) = delete;
  SentReportAccumulator(SentReportAccumulator&&) = delete;

  SentReportAccumulator& operator=(const SentReportAccumulator&) = delete;
  SentReportAccumulator& operator=(SentReportAccumulator&&) = delete;

 private:
  // AttributionManagerImpl::ReportSender:
  void SendReport(AttributionReport report,
                  bool is_debug_report,
                  ReportSentCallback sent_callback) override {
    base::Value report_body = report.ReportBody();
    if (remove_report_ids_)
      report_body.RemoveKey("report_id");

    if (remove_assembled_report_ &&
        absl::holds_alternative<AttributionReport::AggregatableAttributionData>(
            report.data())) {
      report_body.RemoveKey("shared_info");
      report_body.RemoveKey("aggregation_service_payloads");
      report_body.RemoveKey("source_registration_time");
    }

    base::DictionaryValue value;
    value.SetKey("report", std::move(report_body));
    value.SetStringKey("report_url", report.ReportURL(is_debug_report).spec());

    static constexpr char kKeyReportTime[] = "report_time";
    base::TimeDelta report_time_delta = base::Time::Now() - time_origin_;
    switch (report_time_format_) {
      case AttributionReportTimeFormat::kSecondsSinceUnixEpoch:
        value.SetIntKey(kKeyReportTime, report_time_delta.InSeconds());
        break;
      case AttributionReportTimeFormat::kISO8601:
        value.SetStringKey(
            kKeyReportTime,
            base::TimeToISO8601(base::Time::UnixEpoch() + report_time_delta));
        break;
    }

    base::Value::ListStorage* reports;

    base::DictionaryValue test_info;
    if (absl::holds_alternative<AttributionReport::EventLevelData>(
            report.data())) {
      test_info.SetBoolKey(
          "randomized_trigger",
          report.attribution_info().source.attribution_logic() ==
              StoredSource::AttributionLogic::kFalsely);

      reports =
          is_debug_report ? &debug_event_level_reports_ : &event_level_reports_;
    } else {
      auto* aggregatable_data =
          absl::get_if<AttributionReport::AggregatableAttributionData>(
              &report.data());
      DCHECK(aggregatable_data);
      auto list = std::make_unique<base::ListValue>();
      for (const auto& contribution : aggregatable_data->contributions) {
        auto dict = std::make_unique<base::DictionaryValue>();
        // TODO(linnan): Replacing with 128-bit value string.
        dict->SetString(
            "key_high_bits",
            base::NumberToString(absl::Uint128High64(contribution.key())));
        dict->SetString(
            "key_low_bits",
            base::NumberToString(absl::Uint128Low64(contribution.key())));
        dict->SetString("value", base::NumberToString(contribution.value()));

        list->Append(std::move(dict));
      }
      test_info.SetList("histograms", std::move(list));

      reports = is_debug_report ? &debug_aggregatable_reports_
                                : &aggregatable_reports_;
    }
    value.SetKey("test_info", std::move(test_info));

    reports->push_back(std::move(value));

    std::move(sent_callback)
        .Run(std::move(report), SendResult(SendResult::Status::kSent,
                                           /*http_response_code=*/200));
  }

  const base::Time time_origin_;
  const bool remove_report_ids_;
  const AttributionReportTimeFormat report_time_format_;
  const bool remove_assembled_report_;
  base::Value::ListStorage& event_level_reports_;
  base::Value::ListStorage& debug_event_level_reports_;
  base::Value::ListStorage& aggregatable_reports_;
  base::Value::ListStorage& debug_aggregatable_reports_;
};

// Registers sources and triggers in the `AttributionManagerImpl` and records
// rejected sources in a JSON list.
class AttributionEventHandler : public AttributionObserver {
 public:
  AttributionEventHandler(AttributionManagerImpl* manager,
                          base::Value::ListStorage& rejected_sources,
                          base::Value::ListStorage& rejected_triggers)
      : manager_(manager),
        rejected_sources_(rejected_sources),
        rejected_triggers_(rejected_triggers) {
    observation_.Observe(manager);
  }

  ~AttributionEventHandler() override = default;

  void Handle(AttributionSimulationEventAndValue event) {
    // Sources and triggers are handled in order; this includes observer
    // invocations. Therefore, we can track the original `base::Value`
    // associated with the event using a queue.

    input_values_.push_back(std::move(event.second));
    absl::visit(*this, std::move(event.first));
  }

  // For use with `absl::visit()`.
  void operator()(StorableSource source) {
    manager_->MaybeEnqueueEventForTesting(std::move(source));
  }

  // For use with `absl::visit()`.
  void operator()(AttributionTriggerAndTime trigger) {
    manager_->MaybeEnqueueEventForTesting(std::move(trigger.trigger));
  }

 private:
  // AttributionObserver:

  void OnSourceHandled(const StorableSource& source,
                       StorableSource::Result result) override {
    DCHECK(!input_values_.empty());
    base::Value input_value = std::move(input_values_.front());
    input_values_.pop_front();

    std::stringstream reason;
    switch (result) {
      case StorableSource::Result::kSuccess:
        return;
      case StorableSource::Result::kInternalError:
      case StorableSource::Result::kInsufficientSourceCapacity:
      case StorableSource::Result::kInsufficientUniqueDestinationCapacity:
      case StorableSource::Result::kExcessiveReportingOrigins:
      case StorableSource::Result::kProhibitedByBrowserPolicy:
        reason << result;
        break;
    }

    base::DictionaryValue dict;
    dict.SetStringKey("reason", reason.str());
    dict.SetKey("source", std::move(input_value));

    rejected_sources_.push_back(std::move(dict));
  }

  void OnTriggerHandled(const AttributionTrigger& trigger,
                        const CreateReportResult& result) override {
    DCHECK(!input_values_.empty());
    base::Value input_value = std::move(input_values_.front());
    input_values_.pop_front();

    // TODO(linnan): Support aggregatable reports in the simulator.

    std::stringstream event_level_reason;
    switch (result.event_level_status()) {
      case AttributionTrigger::EventLevelResult::kSuccess:
      case AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority:
        // TODO(apaseltiner): Consider surfacing reports dropped due to
        // prioritization.
        break;
      case AttributionTrigger::EventLevelResult::kInternalError:
      case AttributionTrigger::EventLevelResult::
          kNoCapacityForConversionDestination:
      case AttributionTrigger::EventLevelResult::kNoMatchingImpressions:
      case AttributionTrigger::EventLevelResult::kDeduplicated:
      case AttributionTrigger::EventLevelResult::kExcessiveAttributions:
      case AttributionTrigger::EventLevelResult::kPriorityTooLow:
      case AttributionTrigger::EventLevelResult::kDroppedForNoise:
      case AttributionTrigger::EventLevelResult::kExcessiveReportingOrigins:
      case AttributionTrigger::EventLevelResult::kNoMatchingSourceFilterData:
      case AttributionTrigger::EventLevelResult::kProhibitedByBrowserPolicy:
        event_level_reason << result.event_level_status();
        break;
    }

    std::stringstream aggregatable_reason;
    switch (result.aggregatable_status()) {
      case AttributionTrigger::AggregatableResult::kSuccess:
      case AttributionTrigger::AggregatableResult::kNotRegistered:
        break;
      case AttributionTrigger::AggregatableResult::kInternalError:
      case AttributionTrigger::AggregatableResult::
          kNoCapacityForConversionDestination:
      case AttributionTrigger::AggregatableResult::kNoMatchingImpressions:
      case AttributionTrigger::AggregatableResult::kExcessiveAttributions:
      case AttributionTrigger::AggregatableResult::kExcessiveReportingOrigins:
      case AttributionTrigger::AggregatableResult::kInsufficientBudget:
      case AttributionTrigger::AggregatableResult::kNoMatchingSourceFilterData:
      case AttributionTrigger::AggregatableResult::kNoHistograms:
      case AttributionTrigger::AggregatableResult::kProhibitedByBrowserPolicy:
        aggregatable_reason << result.aggregatable_status();
        break;
    }

    std::string event_level_reason_str = event_level_reason.str();
    std::string aggregatable_reason_str = aggregatable_reason.str();

    if (event_level_reason_str.empty() && aggregatable_reason_str.empty())
      return;

    base::DictionaryValue dict;
    if (!event_level_reason_str.empty()) {
      dict.SetStringKey("event_level_reason",
                        std::move(event_level_reason_str));
    }

    if (!aggregatable_reason_str.empty()) {
      dict.SetStringKey("aggregatable_reason",
                        std::move(aggregatable_reason_str));
    }

    dict.SetKey("trigger", std::move(input_value));

    rejected_triggers_.push_back(std::move(dict));
  }

  base::ScopedObservation<AttributionManagerImpl, AttributionObserver>
      observation_{this};

  base::raw_ptr<AttributionManagerImpl> manager_;

  base::Value::ListStorage& rejected_sources_;
  base::Value::ListStorage& rejected_triggers_;

  base::circular_deque<base::Value> input_values_;
};

}  // namespace

base::Value RunAttributionSimulation(
    base::Value input,
    const AttributionSimulationOptions& options,
    std::ostream& error_stream) {
  // Prerequisites for using an environment with mock time.
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestBrowserContext browser_context;

  absl::optional<AttributionSimulationEventAndValues> events =
      ParseAttributionSimulationInput(std::move(input), base::Time::Now(),
                                      error_stream);
  if (!events)
    return base::Value();

  base::ranges::stable_sort(*events, /*comp=*/{}, &GetEventTime);

  // Avoid creating an on-disk sqlite DB.
  content::AttributionManagerImpl::RunInMemoryForTesting();

  // This isn't needed because the DB is completely in memory for testing.
  const base::FilePath user_data_directory;

  std::unique_ptr<AttributionRandomGenerator> rng;
  if (options.noise_seed.has_value()) {
    rng = std::make_unique<AttributionInsecureRandomGenerator>(
        *options.noise_seed);
  } else {
    rng = std::make_unique<AttributionDefaultRandomGenerator>();
  }

  base::Value::ListStorage event_level_reports;
  base::Value::ListStorage debug_event_level_reports;
  base::Value::ListStorage aggregatable_reports;
  base::Value::ListStorage debug_aggregatable_reports;

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      browser_context.GetDefaultStoragePartition());

  auto manager = AttributionManagerImpl::CreateForTesting(
      user_data_directory,
      /*max_pending_events=*/std::numeric_limits<size_t>::max(),
      /*special_storage_policy=*/nullptr,
      AttributionStorageDelegateImpl::CreateForTesting(
          options.noise_mode, options.delay_mode, std::move(rng),
          options.randomized_response_rates),
      std::make_unique<AlwaysSetCookieChecker>(),
      std::make_unique<SentReportAccumulator>(
          event_level_reports, debug_event_level_reports, aggregatable_reports,
          debug_aggregatable_reports, options.remove_report_ids,
          options.report_time_format, options.remove_assembled_report),
      storage_partition);

  base::Value::ListStorage rejected_sources;
  base::Value::ListStorage rejected_triggers;
  AttributionEventHandler handler(manager.get(), rejected_sources,
                                  rejected_triggers);

  storage_partition->GetAggregationService()->SetPublicKeysForTesting(
      GURL(kPrivacySandboxAggregationServiceTrustedServerUrlParam.Get()),
      PublicKeyset({aggregation_service::GenerateKey().public_key},
                   /*fetch_time=*/base::Time::Now(),
                   /*expiry_time=*/base::Time::Max()));

  for (auto& event : *events) {
    task_environment.FastForwardBy(GetEventTime(event) - base::Time::Now());
    handler.Handle(std::move(event));
  }

  std::vector<AttributionReport> pending_reports =
      GetAttributionReportsForTesting(manager.get(),
                                      /*max_report_time=*/base::Time::Max());

  if (!pending_reports.empty()) {
    base::Time last_report_time =
        base::ranges::max(pending_reports, /*comp=*/{},
                          &AttributionReport::report_time)
            .report_time();
    task_environment.FastForwardBy(last_report_time - base::Time::Now());
  }

  base::Value output(base::Value::Type::DICTIONARY);
  output.SetKey("event_level_reports",
                base::Value(std::move(event_level_reports)));

  if (!debug_event_level_reports.empty()) {
    output.SetKey("debug_event_level_reports",
                  base::Value(std::move(debug_event_level_reports)));
  }

  if (!aggregatable_reports.empty()) {
    output.SetKey("aggregatable_reports",
                  base::Value(std::move(aggregatable_reports)));
  }

  if (!debug_aggregatable_reports.empty()) {
    output.SetKey("debug_aggregatable_reports",
                  base::Value(std::move(debug_aggregatable_reports)));
  }

  if (!rejected_sources.empty())
    output.SetKey("rejected_sources", base::Value(std::move(rejected_sources)));

  if (!rejected_triggers.empty()) {
    output.SetKey("rejected_triggers",
                  base::Value(std::move(rejected_triggers)));
  }

  return output;
}

}  // namespace content
