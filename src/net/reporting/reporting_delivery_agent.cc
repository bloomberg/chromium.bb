// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_delivery_agent.h"

#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/network_isolation_key.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_cache_observer.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_endpoint_manager.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_uploader.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

void SerializeReports(const std::vector<const ReportingReport*>& reports,
                      base::TimeTicks now,
                      std::string* json_out) {
  base::ListValue reports_value;

  for (const ReportingReport* report : reports) {
    std::unique_ptr<base::DictionaryValue> report_value =
        std::make_unique<base::DictionaryValue>();

    report_value->SetInteger("age", (now - report->queued).InMilliseconds());
    report_value->SetString("type", report->type);
    report_value->SetString("url", report->url.spec());
    report_value->SetString("user_agent", report->user_agent);
    report_value->SetKey("body", report->body->Clone());

    reports_value.Append(std::move(report_value));
  }

  bool json_written = base::JSONWriter::Write(reports_value, json_out);
  DCHECK(json_written);
}

class ReportingDeliveryAgentImpl : public ReportingDeliveryAgent,
                                   public ReportingCacheObserver {
 public:
  ReportingDeliveryAgentImpl(ReportingContext* context,
                             const RandIntCallback& rand_callback)
      : context_(context),
        timer_(std::make_unique<base::OneShotTimer>()),
        endpoint_manager_(
            ReportingEndpointManager::Create(&context->policy(),
                                             &context->tick_clock(),
                                             context->delegate(),
                                             context->cache(),
                                             rand_callback)) {
    context_->AddCacheObserver(this);
  }

  // ReportingDeliveryAgent implementation:

  ~ReportingDeliveryAgentImpl() override {
    context_->RemoveCacheObserver(this);
  }

  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer) override {
    DCHECK(!timer_->IsRunning());
    timer_ = std::move(timer);
  }

  // ReportingCacheObserver implementation:
  void OnReportsUpdated() override {
    if (CacheHasReports() && !timer_->IsRunning()) {
      SendReports();
      StartTimer();
    }
  }

 private:
  // TODO(chlily): Add NIK.
  using OriginEndpoint = std::pair<url::Origin, GURL>;
  using GroupEndpoint = std::pair<ReportingEndpointGroupKey, GURL>;

  class Delivery {
   public:
    explicit Delivery(const OriginEndpoint& report_origin_endpoint)
        : report_origin(report_origin_endpoint.first),
          endpoint(report_origin_endpoint.second) {}

    ~Delivery() = default;

    void AddReports(const ReportingEndpoint& endpoint,
                    const std::vector<const ReportingReport*>& to_add) {
      GroupEndpoint key = std::make_pair(endpoint.group_key, endpoint.info.url);
      reports_per_endpoint[key] += to_add.size();
      reports.insert(reports.end(), to_add.begin(), to_add.end());
    }

    const url::Origin report_origin;
    const GURL endpoint;
    std::vector<const ReportingReport*> reports;
    std::map<GroupEndpoint, int> reports_per_endpoint;
  };

  bool CacheHasReports() {
    std::vector<const ReportingReport*> reports;
    context_->cache()->GetReports(&reports);
    return !reports.empty();
  }

  void StartTimer() {
    timer_->Start(FROM_HERE, policy().delivery_interval,
                  base::BindOnce(&ReportingDeliveryAgentImpl::OnTimerFired,
                                 base::Unretained(this)));
  }

  void OnTimerFired() {
    if (CacheHasReports()) {
      SendReports();
      StartTimer();
    }
  }

  void SendReports() {
    std::vector<const ReportingReport*> reports =
        cache()->GetReportsToDeliver();

    // First determine which origins we're allowed to upload reports about.
    std::set<url::Origin> report_origins;
    for (const ReportingReport* report : reports) {
      report_origins.insert(url::Origin::Create(report->url));
    }
    delegate()->CanSendReports(
        std::move(report_origins),
        base::BindOnce(&ReportingDeliveryAgentImpl::OnSendPermissionsChecked,
                       weak_factory_.GetWeakPtr(), std::move(reports)));
  }

  void OnSendPermissionsChecked(std::vector<const ReportingReport*> reports,
                                std::set<url::Origin> allowed_report_origins) {
    // Sort reports into buckets by endpoint group.
    std::map<ReportingEndpointGroupKey, std::vector<const ReportingReport*>>
        origin_group_reports;
    for (const ReportingReport* report : reports) {
      url::Origin report_origin = url::Origin::Create(report->url);
      if (allowed_report_origins.find(report_origin) ==
          allowed_report_origins.end()) {
        continue;
      }
      // TODO(chlily): Use proper NIK once reports are double-keyed.
      ReportingEndpointGroupKey group_key(NetworkIsolationKey::Todo(),
                                          report_origin, report->group);
      origin_group_reports[group_key].push_back(report);
    }

    // Find an endpoint for each (origin, group) bucket and sort reports into
    // endpoint buckets. Don't allow concurrent deliveries to the same (origin,
    // group) bucket.
    std::map<OriginEndpoint, std::unique_ptr<Delivery>> deliveries;
    for (auto& it : origin_group_reports) {
      const ReportingEndpointGroupKey& group_key = it.first;

      if (base::Contains(pending_groups_, group_key))
        continue;

      const ReportingEndpoint endpoint =
          endpoint_manager_->FindEndpointForDelivery(group_key);

      if (!endpoint) {
        // TODO(chlily): Remove reports for which there are no valid
        // delivery endpoints.
        continue;
      }
      OriginEndpoint report_origin_endpoint(group_key.origin,
                                            endpoint.info.url);

      Delivery* delivery;
      auto delivery_it = deliveries.find(report_origin_endpoint);
      if (delivery_it == deliveries.end()) {
        auto new_delivery = std::make_unique<Delivery>(report_origin_endpoint);
        delivery = new_delivery.get();
        deliveries[report_origin_endpoint] = std::move(new_delivery);
      } else {
        delivery = delivery_it->second.get();
      }

      delivery->AddReports(endpoint, it.second);
      pending_groups_.insert(group_key);
    }

    // Keep track of which of these reports we don't queue for delivery; we'll
    // need to mark them as not-pending.
    std::unordered_set<const ReportingReport*> undelivered_reports(
        reports.begin(), reports.end());

    // Start an upload for each delivery.
    for (auto& it : deliveries) {
      const OriginEndpoint& report_origin_endpoint = it.first;
      const url::Origin& report_origin = report_origin_endpoint.first;
      const GURL& endpoint = report_origin_endpoint.second;
      std::unique_ptr<Delivery>& delivery = it.second;

      std::string json;
      SerializeReports(delivery->reports, tick_clock().NowTicks(), &json);

      int max_depth = 0;
      for (const ReportingReport* report : delivery->reports) {
        undelivered_reports.erase(report);
        if (report->depth > max_depth)
          max_depth = report->depth;
      }

      // TODO: Calculate actual max depth.
      // TODO(mmenke): Populate NetworkIsolationKey.
      uploader()->StartUpload(
          report_origin, endpoint, NetworkIsolationKey::Todo(), json, max_depth,
          base::BindOnce(&ReportingDeliveryAgentImpl::OnUploadComplete,
                         weak_factory_.GetWeakPtr(), std::move(delivery)));
    }

    cache()->ClearReportsPending(
        {undelivered_reports.begin(), undelivered_reports.end()});
  }

  void OnUploadComplete(std::unique_ptr<Delivery> delivery,
                        ReportingUploader::Outcome outcome) {
    for (const auto& endpoint_and_count : delivery->reports_per_endpoint) {
      const ReportingEndpointGroupKey& group_key =
          endpoint_and_count.first.first;
      const GURL& endpoint = endpoint_and_count.first.second;
      int report_count = endpoint_and_count.second;
      cache()->IncrementEndpointDeliveries(
          group_key, endpoint, report_count,
          outcome == ReportingUploader::Outcome::SUCCESS);
    }

    if (outcome == ReportingUploader::Outcome::SUCCESS) {
      cache()->RemoveReports(delivery->reports,
                             ReportingReport::Outcome::DELIVERED);
      // TODO(mmenke): Populate NetworkIsolationKey argument.
      endpoint_manager_->InformOfEndpointRequest(NetworkIsolationKey::Todo(),
                                                 delivery->endpoint, true);
    } else {
      cache()->IncrementReportsAttempts(delivery->reports);
      // TODO(mmenke): Populate NetworkIsolationKey argument.
      endpoint_manager_->InformOfEndpointRequest(NetworkIsolationKey::Todo(),
                                                 delivery->endpoint, false);
    }

    if (outcome == ReportingUploader::Outcome::REMOVE_ENDPOINT)
      cache()->RemoveEndpointsForUrl(delivery->endpoint);

    for (const ReportingReport* report : delivery->reports) {
      ReportingEndpointGroupKey group_key(
          NetworkIsolationKey::Todo(), delivery->report_origin, report->group);
      pending_groups_.erase(group_key);
    }

    cache()->ClearReportsPending(delivery->reports);
  }

  const ReportingPolicy& policy() const { return context_->policy(); }
  const base::TickClock& tick_clock() const { return context_->tick_clock(); }
  ReportingDelegate* delegate() { return context_->delegate(); }
  ReportingCache* cache() { return context_->cache(); }
  ReportingUploader* uploader() { return context_->uploader(); }

  ReportingContext* context_;

  std::unique_ptr<base::OneShotTimer> timer_;

  // Tracks endpoint groups for which there is a pending delivery running.
  std::set<ReportingEndpointGroupKey> pending_groups_;

  std::unique_ptr<ReportingEndpointManager> endpoint_manager_;

  base::WeakPtrFactory<ReportingDeliveryAgentImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ReportingDeliveryAgentImpl);
};

}  // namespace

// static
std::unique_ptr<ReportingDeliveryAgent> ReportingDeliveryAgent::Create(
    ReportingContext* context,
    const RandIntCallback& rand_callback) {
  return std::make_unique<ReportingDeliveryAgentImpl>(context, rand_callback);
}

ReportingDeliveryAgent::~ReportingDeliveryAgent() = default;

}  // namespace net
