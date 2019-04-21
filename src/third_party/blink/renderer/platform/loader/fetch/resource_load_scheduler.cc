// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/aggregated_metric_reporter.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_status.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

namespace {

// Field trial name for throttling.
const char kResourceLoadThrottlingTrial[] = "ResourceLoadScheduler";

// Field trial parameter names.
// Note: bg_limit is supported on m61+, but bg_sub_limit is only on m63+.
// If bg_sub_limit param is not found, we should use bg_limit to make the
// study result statistically correct.
const char kOutstandingLimitForBackgroundMainFrameName[] = "bg_limit";
const char kOutstandingLimitForBackgroundSubFrameName[] = "bg_sub_limit";

// Field trial default parameters.
constexpr size_t kOutstandingLimitForBackgroundMainFrameDefault = 3u;
constexpr size_t kOutstandingLimitForBackgroundSubFrameDefault = 2u;

// Maximum request count that request count metrics assume.
constexpr base::HistogramBase::Sample kMaximumReportSize10K = 10000;

// Maximum traffic bytes that traffic metrics assume.
constexpr base::HistogramBase::Sample kMaximumReportSize1G =
    1 * 1000 * 1000 * 1000;

// Bucket count for metrics.
constexpr int32_t kReportBucketCount = 25;

constexpr char kRendererSideResourceScheduler[] =
    "RendererSideResourceScheduler";

// Used in the tight mode (see the header file for details).
constexpr size_t kTightLimitForRendererSideResourceScheduler = 2u;
// Used in the normal mode (see the header file for details).
constexpr size_t kLimitForRendererSideResourceScheduler = 1024u;

constexpr char kTightLimitForRendererSideResourceSchedulerName[] =
    "tight_limit";
constexpr char kLimitForRendererSideResourceSchedulerName[] = "limit";

// Represents a resource load circumstance, e.g. from main frame vs sub-frames,
// or on throttled state vs on not-throttled state.
// Used to report histograms. Do not reorder or insert new items.
enum class ReportCircumstance {
  kMainframeThrottled,
  kMainframeNotThrottled,
  kSubframeThrottled,
  kSubframeNotThrottled,
  // Append new items here.
  kNumOfCircumstances,
};

base::HistogramBase::Sample ToSample(ReportCircumstance circumstance) {
  return static_cast<base::HistogramBase::Sample>(circumstance);
}

uint32_t GetFieldTrialUint32Param(const char* trial_name,
                                  const char* parameter_name,
                                  uint32_t default_param) {
  std::map<std::string, std::string> trial_params;
  bool result = base::GetFieldTrialParams(trial_name, &trial_params);
  if (!result)
    return default_param;

  const auto& found = trial_params.find(parameter_name);
  if (found == trial_params.end())
    return default_param;

  uint32_t param;
  if (!base::StringToUint(found->second, &param))
    return default_param;

  return param;
}

size_t GetOutstandingThrottledLimit(
    const ResourceFetcherProperties& properties) {
  if (!RuntimeEnabledFeatures::ResourceLoadSchedulerEnabled())
    return ResourceLoadScheduler::kOutstandingUnlimited;

  static size_t main_frame_limit = GetFieldTrialUint32Param(
      kResourceLoadThrottlingTrial, kOutstandingLimitForBackgroundMainFrameName,
      kOutstandingLimitForBackgroundMainFrameDefault);
  static size_t sub_frame_limit = GetFieldTrialUint32Param(
      kResourceLoadThrottlingTrial, kOutstandingLimitForBackgroundSubFrameName,
      kOutstandingLimitForBackgroundSubFrameDefault);

  return properties.IsMainFrame() ? main_frame_limit : sub_frame_limit;
}

int TakeWholeKilobytes(int64_t& bytes) {
  int kilobytes = base::saturated_cast<int>(bytes / 1024);
  bytes %= 1024;
  return kilobytes;
}

bool IsResourceLoadThrottlingEnabled() {
  return RuntimeEnabledFeatures::ResourceLoadSchedulerEnabled();
}

}  // namespace

// A class to gather throttling and traffic information to report histograms.
class ResourceLoadScheduler::TrafficMonitor {
 public:
  explicit TrafficMonitor(
      const ResourceFetcherProperties& resource_fetcher_properties);
  ~TrafficMonitor();

  // Notified when the ThrottlingState is changed.
  void OnLifecycleStateChanged(scheduler::SchedulingLifecycleState);

  // Reports resource request completion.
  void Report(const ResourceLoadScheduler::TrafficReportHints&);

  // Reports per-frame reports.
  void ReportAll();

 private:
  const Persistent<const ResourceFetcherProperties>
      resource_fetcher_properties_;

  scheduler::SchedulingLifecycleState current_state_ =
      scheduler::SchedulingLifecycleState::kStopped;

  uint32_t total_throttled_request_count_ = 0;
  size_t total_throttled_traffic_bytes_ = 0;
  size_t total_throttled_decoded_bytes_ = 0;
  uint32_t total_not_throttled_request_count_ = 0;
  size_t total_not_throttled_traffic_bytes_ = 0;
  size_t total_not_throttled_decoded_bytes_ = 0;
  uint32_t throttling_state_change_count_ = 0;
  bool report_all_is_called_ = false;

  scheduler::AggregatedMetricReporter<scheduler::FrameStatus, int64_t>
      traffic_kilobytes_per_frame_status_;
  scheduler::AggregatedMetricReporter<scheduler::FrameStatus, int64_t>
      decoded_kilobytes_per_frame_status_;
};

ResourceLoadScheduler::TrafficMonitor::TrafficMonitor(
    const ResourceFetcherProperties& resource_fetcher_properties)
    : resource_fetcher_properties_(resource_fetcher_properties),
      traffic_kilobytes_per_frame_status_(
          "Blink.ResourceLoadScheduler.TrafficBytes.KBPerFrameStatus",
          &TakeWholeKilobytes),
      decoded_kilobytes_per_frame_status_(
          "Blink.ResourceLoadScheduler.DecodedBytes.KBPerFrameStatus",
          &TakeWholeKilobytes) {}

ResourceLoadScheduler::TrafficMonitor::~TrafficMonitor() {
  ReportAll();
}

void ResourceLoadScheduler::TrafficMonitor::OnLifecycleStateChanged(
    scheduler::SchedulingLifecycleState state) {
  current_state_ = state;
  throttling_state_change_count_++;
}

void ResourceLoadScheduler::TrafficMonitor::Report(
    const ResourceLoadScheduler::TrafficReportHints& hints) {
  // Currently we only care about stats from frames.
  if (!IsMainThread())
    return;
  if (!hints.IsValid())
    return;

  DEFINE_STATIC_LOCAL(EnumerationHistogram, request_count_by_circumstance,
                      ("Blink.ResourceLoadScheduler.RequestCount",
                       ToSample(ReportCircumstance::kNumOfCircumstances)));

  switch (current_state_) {
    case scheduler::SchedulingLifecycleState::kThrottled:
    case scheduler::SchedulingLifecycleState::kHidden:
      if (resource_fetcher_properties_->IsMainFrame()) {
        request_count_by_circumstance.Count(
            ToSample(ReportCircumstance::kMainframeThrottled));
      } else {
        request_count_by_circumstance.Count(
            ToSample(ReportCircumstance::kSubframeThrottled));
      }
      total_throttled_request_count_++;
      total_throttled_traffic_bytes_ += hints.encoded_data_length();
      total_throttled_decoded_bytes_ += hints.decoded_body_length();
      break;
    case scheduler::SchedulingLifecycleState::kNotThrottled:
      if (resource_fetcher_properties_->IsMainFrame()) {
        request_count_by_circumstance.Count(
            ToSample(ReportCircumstance::kMainframeNotThrottled));
      } else {
        request_count_by_circumstance.Count(
            ToSample(ReportCircumstance::kSubframeNotThrottled));
      }
      total_not_throttled_request_count_++;
      total_not_throttled_traffic_bytes_ += hints.encoded_data_length();
      total_not_throttled_decoded_bytes_ += hints.decoded_body_length();
      break;
    case scheduler::SchedulingLifecycleState::kStopped:
      break;
  }

  // Report kilobytes instead of bytes to avoid overflows.
  int64_t encoded_kilobytes = hints.encoded_data_length() / 1024;
  int64_t decoded_kilobytes = hints.decoded_body_length() / 1024;

  if (encoded_kilobytes) {
    traffic_kilobytes_per_frame_status_.RecordTask(
        resource_fetcher_properties_->GetFrameStatus(), encoded_kilobytes);
  }
  if (decoded_kilobytes) {
    decoded_kilobytes_per_frame_status_.RecordTask(
        resource_fetcher_properties_->GetFrameStatus(), decoded_kilobytes);
  }
}

void ResourceLoadScheduler::TrafficMonitor::ReportAll() {
  // Currently we only care about stats from frames.
  if (!IsMainThread())
    return;

  // Blink has several cases to create DocumentLoader not for an actual page
  // load use. I.e., per a XMLHttpRequest in "document" type response.
  // We just ignore such uninteresting cases in following metrics.
  if (!total_throttled_request_count_ && !total_not_throttled_request_count_)
    return;

  if (report_all_is_called_)
    return;
  report_all_is_called_ = true;

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_throttled_request_count,
      ("Blink.ResourceLoadScheduler.TotalRequestCount.MainframeThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_not_throttled_request_count,
      ("Blink.ResourceLoadScheduler.TotalRequestCount.MainframeNotThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_throttled_request_count,
      ("Blink.ResourceLoadScheduler.TotalRequestCount.SubframeThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_not_throttled_request_count,
      ("Blink.ResourceLoadScheduler.TotalRequestCount.SubframeNotThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TotalTrafficBytes.MainframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_not_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TotalTrafficBytes.MainframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TotalTrafficBytes.SubframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_not_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TotalTrafficBytes.SubframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.TotalDecodedBytes.MainframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_not_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.TotalDecodedBytes.MainframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.TotalDecodedBytes.SubframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_not_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.TotalDecodedBytes.SubframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));

  DEFINE_STATIC_LOCAL(CustomCountHistogram, throttling_state_change_count,
                      ("Blink.ResourceLoadScheduler.ThrottlingStateChangeCount",
                       0, 100, kReportBucketCount));

  if (resource_fetcher_properties_->IsMainFrame()) {
    main_frame_total_throttled_request_count.Count(
        total_throttled_request_count_);
    main_frame_total_not_throttled_request_count.Count(
        total_not_throttled_request_count_);
    main_frame_total_throttled_traffic_bytes.Count(
        base::saturated_cast<base::Histogram::Sample>(
            total_throttled_traffic_bytes_));
    main_frame_total_not_throttled_traffic_bytes.Count(
        base::saturated_cast<base::Histogram::Sample>(
            total_not_throttled_traffic_bytes_));
    main_frame_total_throttled_decoded_bytes.Count(
        base::saturated_cast<base::Histogram::Sample>(
            total_throttled_decoded_bytes_));
    main_frame_total_not_throttled_decoded_bytes.Count(
        base::saturated_cast<base::Histogram::Sample>(
            total_not_throttled_decoded_bytes_));
  } else {
    sub_frame_total_throttled_request_count.Count(
        total_throttled_request_count_);
    sub_frame_total_not_throttled_request_count.Count(
        total_not_throttled_request_count_);
    sub_frame_total_throttled_traffic_bytes.Count(
        base::saturated_cast<base::Histogram::Sample>(
            total_throttled_traffic_bytes_));
    sub_frame_total_not_throttled_traffic_bytes.Count(
        base::saturated_cast<base::Histogram::Sample>(
            total_not_throttled_traffic_bytes_));
    sub_frame_total_throttled_decoded_bytes.Count(
        base::saturated_cast<base::Histogram::Sample>(
            total_throttled_decoded_bytes_));
    sub_frame_total_not_throttled_decoded_bytes.Count(
        base::saturated_cast<base::Histogram::Sample>(
            total_not_throttled_decoded_bytes_));
  }

  throttling_state_change_count.Count(throttling_state_change_count_);
}

constexpr ResourceLoadScheduler::ClientId
    ResourceLoadScheduler::kInvalidClientId;

ResourceLoadScheduler::ResourceLoadScheduler(
    ThrottlingPolicy initial_throttling_policy,
    const ResourceFetcherProperties& resource_fetcher_properties,
    FrameScheduler* frame_scheduler,
    ConsoleLogger& console_logger)
    : resource_fetcher_properties_(resource_fetcher_properties),
      policy_(initial_throttling_policy),
      outstanding_limit_for_throttled_frame_scheduler_(
          GetOutstandingThrottledLimit(*resource_fetcher_properties_)),
      console_logger_(console_logger) {
  traffic_monitor_ = std::make_unique<ResourceLoadScheduler::TrafficMonitor>(
      resource_fetcher_properties);

  if (!frame_scheduler)
    return;

  normal_outstanding_limit_ =
      GetFieldTrialUint32Param(kRendererSideResourceScheduler,
                               kLimitForRendererSideResourceSchedulerName,
                               kLimitForRendererSideResourceScheduler);
  tight_outstanding_limit_ =
      GetFieldTrialUint32Param(kRendererSideResourceScheduler,
                               kTightLimitForRendererSideResourceSchedulerName,
                               kTightLimitForRendererSideResourceScheduler);

  scheduler_observer_handle_ = frame_scheduler->AddLifecycleObserver(
      FrameScheduler::ObserverType::kLoader, this);
}

ResourceLoadScheduler::~ResourceLoadScheduler() = default;

void ResourceLoadScheduler::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_request_map_);
  visitor->Trace(resource_fetcher_properties_);
  visitor->Trace(console_logger_);
}

void ResourceLoadScheduler::LoosenThrottlingPolicy() {
  switch (policy_) {
    case ThrottlingPolicy::kTight:
      break;
    case ThrottlingPolicy::kNormal:
      return;
  }
  policy_ = ThrottlingPolicy::kNormal;
  MaybeRun();
}

void ResourceLoadScheduler::Shutdown() {
  // Do nothing if the feature is not enabled, or Shutdown() was already called.
  if (is_shutdown_)
    return;
  is_shutdown_ = true;

  if (traffic_monitor_)
    traffic_monitor_.reset();

  scheduler_observer_handle_.reset();
}

void ResourceLoadScheduler::Request(ResourceLoadSchedulerClient* client,
                                    ThrottleOption option,
                                    ResourceLoadPriority priority,
                                    int intra_priority,
                                    ResourceLoadScheduler::ClientId* id) {
  *id = GenerateClientId();
  if (is_shutdown_)
    return;

  // Check if the request can be throttled.
  ClientIdWithPriority request_info(*id, priority, intra_priority);
  if (!IsClientDelayable(request_info, option)) {
    Run(*id, client, false);
    return;
  }

  DCHECK(ThrottleOption::kStoppable == option ||
         ThrottleOption::kThrottleable == option);
  if (pending_requests_[option].empty())
    pending_queue_update_times_[option] = CurrentTime();
  pending_requests_[option].insert(request_info);
  pending_request_map_.insert(
      *id, MakeGarbageCollected<ClientInfo>(client, option, priority,
                                            intra_priority));

  // Remember the ClientId since MaybeRun() below may destruct the caller
  // instance and |id| may be inaccessible after the call.
  MaybeRun();
}

void ResourceLoadScheduler::SetPriority(ClientId client_id,
                                        ResourceLoadPriority priority,
                                        int intra_priority) {
  auto client_it = pending_request_map_.find(client_id);
  if (client_it == pending_request_map_.end())
    return;

  auto& throttle_option_queue = pending_requests_[client_it->value->option];

  auto it = throttle_option_queue.find(ClientIdWithPriority(
      client_id, client_it->value->priority, client_it->value->intra_priority));

  DCHECK(it != throttle_option_queue.end());
  throttle_option_queue.erase(it);

  client_it->value->priority = priority;
  client_it->value->intra_priority = intra_priority;

  throttle_option_queue.emplace(client_id, priority, intra_priority);
  MaybeRun();
}

bool ResourceLoadScheduler::Release(
    ResourceLoadScheduler::ClientId id,
    ResourceLoadScheduler::ReleaseOption option,
    const ResourceLoadScheduler::TrafficReportHints& hints) {
  // Check kInvalidClientId that can not be passed to the HashSet.
  if (id == kInvalidClientId)
    return false;

  if (running_requests_.Contains(id)) {
    running_requests_.erase(id);
    running_throttleable_requests_.erase(id);

    if (traffic_monitor_)
      traffic_monitor_->Report(hints);

    if (option == ReleaseOption::kReleaseAndSchedule)
      MaybeRun();
    return true;
  }
  auto found = pending_request_map_.find(id);
  if (found != pending_request_map_.end()) {
    pending_request_map_.erase(found);
    // Intentionally does not remove it from |pending_requests_|.

    // Didn't release any running requests, but the outstanding limit might be
    // changed to allow another request.
    if (option == ReleaseOption::kReleaseAndSchedule)
      MaybeRun();
    return true;
  }
  return false;
}

void ResourceLoadScheduler::SetOutstandingLimitForTesting(size_t tight_limit,
                                                          size_t normal_limit) {
  tight_outstanding_limit_ = tight_limit;
  normal_outstanding_limit_ = normal_limit;
  MaybeRun();
}

void ResourceLoadScheduler::OnNetworkQuiet() {
  DCHECK(IsMainThread());

  // Flush out all traffic reports here for safety.
  traffic_monitor_->ReportAll();

  if (maximum_running_requests_seen_ == 0)
    return;

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_throttled,
      ("Blink.ResourceLoadScheduler.PeakRequests.MainframeThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_not_throttled,
      ("Blink.ResourceLoadScheduler.PeakRequests.MainframeNotThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_throttled,
      ("Blink.ResourceLoadScheduler.PeakRequests.SubframeThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_not_throttled,
      ("Blink.ResourceLoadScheduler.PeakRequests.SubframeNotThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));

  switch (throttling_history_) {
    case ThrottlingHistory::kInitial:
    case ThrottlingHistory::kNotThrottled:
      if (resource_fetcher_properties_->IsMainFrame())
        main_frame_not_throttled.Count(maximum_running_requests_seen_);
      else
        sub_frame_not_throttled.Count(maximum_running_requests_seen_);
      break;
    case ThrottlingHistory::kThrottled:
      if (resource_fetcher_properties_->IsMainFrame())
        main_frame_throttled.Count(maximum_running_requests_seen_);
      else
        sub_frame_throttled.Count(maximum_running_requests_seen_);
      break;
    case ThrottlingHistory::kPartiallyThrottled:
      break;
    case ThrottlingHistory::kStopped:
      break;
  }
}

bool ResourceLoadScheduler::IsClientDelayable(const ClientIdWithPriority& info,
                                              ThrottleOption option) const {
  const bool throttleable = option == ThrottleOption::kThrottleable &&
                            info.priority < ResourceLoadPriority::kHigh;
  const bool stoppable = option != ThrottleOption::kCanNotBeStoppedOrThrottled;

  // Also takes the lifecycle state of the associated FrameScheduler
  // into account to determine if the request should be throttled
  // regardless of the priority.
  switch (frame_scheduler_lifecycle_state_) {
    case scheduler::SchedulingLifecycleState::kNotThrottled:
      return throttleable;
    case scheduler::SchedulingLifecycleState::kHidden:
    case scheduler::SchedulingLifecycleState::kThrottled:
      if (IsResourceLoadThrottlingEnabled())
        return option == ThrottleOption::kThrottleable;
      return throttleable;
    case scheduler::SchedulingLifecycleState::kStopped:
      return stoppable;
  }

  NOTREACHED() << static_cast<int>(frame_scheduler_lifecycle_state_);
  return throttleable;
}

void ResourceLoadScheduler::OnLifecycleStateChanged(
    scheduler::SchedulingLifecycleState state) {
  if (frame_scheduler_lifecycle_state_ == state)
    return;

  if (traffic_monitor_)
    traffic_monitor_->OnLifecycleStateChanged(state);

  frame_scheduler_lifecycle_state_ = state;

  switch (state) {
    case scheduler::SchedulingLifecycleState::kHidden:
    case scheduler::SchedulingLifecycleState::kThrottled:
      if (throttling_history_ == ThrottlingHistory::kInitial)
        throttling_history_ = ThrottlingHistory::kThrottled;
      else if (throttling_history_ == ThrottlingHistory::kNotThrottled)
        throttling_history_ = ThrottlingHistory::kPartiallyThrottled;
      break;
    case scheduler::SchedulingLifecycleState::kNotThrottled:
      if (throttling_history_ == ThrottlingHistory::kInitial)
        throttling_history_ = ThrottlingHistory::kNotThrottled;
      else if (throttling_history_ == ThrottlingHistory::kThrottled)
        throttling_history_ = ThrottlingHistory::kPartiallyThrottled;
      ShowConsoleMessageIfNeeded();
      break;
    case scheduler::SchedulingLifecycleState::kStopped:
      throttling_history_ = ThrottlingHistory::kStopped;
      break;
  }
  MaybeRun();
}

ResourceLoadScheduler::ClientId ResourceLoadScheduler::GenerateClientId() {
  ClientId id = ++current_id_;
  CHECK_NE(0u, id);
  return id;
}

bool ResourceLoadScheduler::IsPendingRequestEffectivelyEmpty(
    ThrottleOption option) {
  for (const auto& client : pending_requests_[option]) {
    // The request in |pending_request_| is erased when it is scheduled. So if
    // the request is canceled, or Release() is called before firing its Run(),
    // the entry for the request remains in |pending_request_| until it is
    // popped in GetNextPendingRequest().
    if (pending_request_map_.find(client.client_id) !=
        pending_request_map_.end()) {
      return false;
    }
  }
  // There is no entry, or no existing entries are alive in
  // |pending_request_map_|.
  return true;
}

bool ResourceLoadScheduler::GetNextPendingRequest(ClientId* id) {
  bool needs_throttling =
      running_throttleable_requests_.size() >= GetOutstandingLimit();

  auto& stoppable_queue = pending_requests_[ThrottleOption::kStoppable];
  auto& throttleable_queue = pending_requests_[ThrottleOption::kThrottleable];

  // Check if stoppable or throttleable requests are allowed to be run.
  auto stoppable_it = stoppable_queue.begin();
  bool has_runnable_stoppable_request =
      stoppable_it != stoppable_queue.end() &&
      (!IsClientDelayable(*stoppable_it, ThrottleOption::kStoppable) ||
       !needs_throttling);

  auto throttleable_it = throttleable_queue.begin();
  bool has_runnable_throttleable_request =
      throttleable_it != throttleable_queue.end() &&
      (!IsClientDelayable(*throttleable_it, ThrottleOption::kThrottleable) ||
       !needs_throttling);

  if (!has_runnable_throttleable_request && !has_runnable_stoppable_request)
    return false;

  // If both requests are allowed to be run, run the high priority requests
  // first.
  ClientIdWithPriority::Compare compare;
  bool use_stoppable = has_runnable_stoppable_request &&
                       (!has_runnable_throttleable_request ||
                        compare(*stoppable_it, *throttleable_it));

  // Remove the iterator from the correct set of |pending_requests_|, and update
  // corresponding |pending_queue_update_times_|.
  if (use_stoppable) {
    *id = stoppable_it->client_id;
    stoppable_queue.erase(stoppable_it);
    pending_queue_update_times_[ThrottleOption::kStoppable] = CurrentTime();
    return true;
  }
  *id = throttleable_it->client_id;
  throttleable_queue.erase(throttleable_it);
  pending_queue_update_times_[ThrottleOption::kThrottleable] = CurrentTime();
  return true;
}

void ResourceLoadScheduler::MaybeRun() {
  // Requests for keep-alive loaders could be remained in the pending queue,
  // but ignore them once Shutdown() is called.
  if (is_shutdown_)
    return;

  ClientId id = kInvalidClientId;
  while (GetNextPendingRequest(&id)) {
    auto found = pending_request_map_.find(id);
    if (found == pending_request_map_.end())
      continue;  // Already released.
    ResourceLoadSchedulerClient* client = found->value->client;
    ThrottleOption option = found->value->option;
    pending_request_map_.erase(found);
    Run(id, client, option == ThrottleOption::kThrottleable);
  }
}

void ResourceLoadScheduler::Run(ResourceLoadScheduler::ClientId id,
                                ResourceLoadSchedulerClient* client,
                                bool throttleable) {
  running_requests_.insert(id);
  if (throttleable)
    running_throttleable_requests_.insert(id);
  if (running_requests_.size() > maximum_running_requests_seen_) {
    maximum_running_requests_seen_ = running_requests_.size();
  }
  client->Run();
}

size_t ResourceLoadScheduler::GetOutstandingLimit() const {
  size_t limit = kOutstandingUnlimited;

  switch (frame_scheduler_lifecycle_state_) {
    case scheduler::SchedulingLifecycleState::kHidden:
    case scheduler::SchedulingLifecycleState::kThrottled:
      limit = std::min(limit, outstanding_limit_for_throttled_frame_scheduler_);
      break;
    case scheduler::SchedulingLifecycleState::kNotThrottled:
      break;
    case scheduler::SchedulingLifecycleState::kStopped:
      limit = 0;
      break;
  }

  switch (policy_) {
    case ThrottlingPolicy::kTight:
      limit = std::min(limit, tight_outstanding_limit_);
      break;
    case ThrottlingPolicy::kNormal:
      limit = std::min(limit, normal_outstanding_limit_);
      break;
  }
  return limit;
}

void ResourceLoadScheduler::ShowConsoleMessageIfNeeded() {
  if (is_console_info_shown_ || pending_request_map_.IsEmpty())
    return;

  const double limit = CurrentTime() - 60;  // In seconds
  ThrottleOption target_option;
  if (pending_queue_update_times_[ThrottleOption::kThrottleable] < limit &&
      !IsPendingRequestEffectivelyEmpty(ThrottleOption::kThrottleable)) {
    target_option = ThrottleOption::kThrottleable;
  } else if (pending_queue_update_times_[ThrottleOption::kStoppable] < limit &&
             !IsPendingRequestEffectivelyEmpty(ThrottleOption::kStoppable)) {
    target_option = ThrottleOption::kStoppable;
  } else {
    // At least, one of the top requests in pending queues was handled in the
    // last 1 minutes, or there is no pending requests in the inactive queue.
    return;
  }
  console_logger_->AddConsoleMessage(
      mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kInfo,
      "Some resource load requests were throttled while the tab was in "
      "background, and no request was sent from the queue in the last 1 "
      "minute. This means previously requested in-flight requests haven't "
      "received any response from servers. See "
      "https://www.chromestatus.com/feature/5527160148197376 for more details");
  is_console_info_shown_ = true;
}

}  // namespace blink
