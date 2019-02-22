// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"

namespace data_reduction_proxy {

// A |net::URLRequestContextGetter| which uses only vanilla HTTP/HTTPS for
// performing requests. This is used by the secure proxy check to prevent the
// use of SPDY and QUIC which may be used by the primary request contexts.
class BasicHTTPURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  BasicHTTPURLRequestContextGetter(
      const std::string& user_agent,
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner);

  // Overridden from net::URLRequestContextGetter:
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 private:
  ~BasicHTTPURLRequestContextGetter() override;

  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  std::unique_ptr<net::HttpUserAgentSettings> user_agent_settings_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(BasicHTTPURLRequestContextGetter);
};

BasicHTTPURLRequestContextGetter::BasicHTTPURLRequestContextGetter(
    const std::string& user_agent,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner)
    : network_task_runner_(network_task_runner),
      user_agent_settings_(
          new net::StaticHttpUserAgentSettings(std::string(), user_agent)) {
}

net::URLRequestContext*
BasicHTTPURLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_) {
    net::URLRequestContextBuilder builder;
    builder.set_proxy_resolution_service(net::ProxyResolutionService::CreateDirect());
    builder.SetSpdyAndQuicEnabled(false, false);
    url_request_context_ = builder.Build();
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
BasicHTTPURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

BasicHTTPURLRequestContextGetter::~BasicHTTPURLRequestContextGetter() {
}

DataReductionProxyIOData::DataReductionProxyIOData(
    Client client,
    PrefService* prefs,
    net::NetLog* net_log,
    network::NetworkConnectionTracker* network_connection_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    bool enabled,
    const std::string& user_agent,
    const std::string& channel)
    : client_(client),
      net_log_(net_log),
      network_connection_tracker_(network_connection_tracker),
      io_task_runner_(io_task_runner),
      ui_task_runner_(ui_task_runner),
      data_use_observer_(nullptr),
      enabled_(enabled),
      url_request_context_getter_(nullptr),
      basic_url_request_context_getter_(
          new BasicHTTPURLRequestContextGetter(user_agent, io_task_runner)),
      channel_(channel),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      weak_factory_(this) {
  DCHECK(net_log);
  DCHECK(io_task_runner_);
  DCHECK(ui_task_runner_);
  event_creator_.reset(new DataReductionProxyEventCreator(this));
  configurator_.reset(
      new DataReductionProxyConfigurator(net_log, event_creator_.get()));
  configurator_->SetConfigUpdatedCallback(base::BindRepeating(
      &DataReductionProxyIOData::OnProxyConfigUpdated, base::Unretained(this)));
  DataReductionProxyMutableConfigValues* raw_mutable_config = nullptr;
    std::unique_ptr<DataReductionProxyMutableConfigValues> mutable_config =
        std::make_unique<DataReductionProxyMutableConfigValues>();
    raw_mutable_config = mutable_config.get();
    config_.reset(new DataReductionProxyConfig(
        io_task_runner, net_log, network_connection_tracker_,
        std::move(mutable_config), configurator_.get(), event_creator_.get()));

  // It is safe to use base::Unretained here, since it gets executed
  // synchronously on the IO thread, and |this| outlives the caller (since the
  // caller is owned by |this|.
  bypass_stats_.reset(new DataReductionProxyBypassStats(
      config_.get(),
      base::BindRepeating(&DataReductionProxyIOData::SetUnreachable,
                          base::Unretained(this)),
      network_connection_tracker_));
  request_options_.reset(
      new DataReductionProxyRequestOptions(client_, config_.get()));
  request_options_->Init();
  // It is safe to use base::Unretained here, since it gets executed
  // synchronously on the IO thread, and |this| outlives the caller (since the
  // caller is owned by |this|.
  request_options_->SetUpdateHeaderCallback(
      base::BindRepeating(&DataReductionProxyIOData::UpdateProxyRequestHeaders,
                          base::Unretained(this)));

    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives the caller (since the
    // caller is owned by |this|.
  config_client_.reset(new DataReductionProxyConfigServiceClient(
      GetBackoffPolicy(), request_options_.get(), raw_mutable_config,
      config_.get(), event_creator_.get(), this, net_log_,
      network_connection_tracker_,
      base::BindRepeating(&DataReductionProxyIOData::StoreSerializedConfig,
                          base::Unretained(this))));

  proxy_delegate_.reset(new DataReductionProxyDelegate(
      config_.get(), configurator_.get(), event_creator_.get(),
      bypass_stats_.get(), net_log_));
  network_properties_manager_.reset(new NetworkPropertiesManager(
      base::DefaultClock::GetInstance(), prefs, ui_task_runner_));
}

DataReductionProxyIOData::DataReductionProxyIOData(
    PrefService* prefs,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : client_(Client::UNKNOWN),
      net_log_(nullptr),
      network_connection_tracker_(nullptr),
      io_task_runner_(io_task_runner),
      ui_task_runner_(ui_task_runner),
      url_request_context_getter_(nullptr),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      weak_factory_(this) {
  DCHECK(ui_task_runner_);
  DCHECK(io_task_runner_);
  network_properties_manager_.reset(new NetworkPropertiesManager(
      base::DefaultClock::GetInstance(), prefs, ui_task_runner_));
}

DataReductionProxyIOData::~DataReductionProxyIOData() {
  // Guaranteed to be destroyed on IO thread if the IO thread is still
  // available at the time of destruction. If the IO thread is unavailable,
  // then the destruction will happen on the UI thread.
}

void DataReductionProxyIOData::ShutdownOnUIThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  network_properties_manager_->ShutdownOnUIThread();
}

void DataReductionProxyIOData::SetDataReductionProxyService(
    base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  service_ = data_reduction_proxy_service;
  url_request_context_getter_ = service_->url_request_context_getter();
  url_loader_factory_info_ = service_->url_loader_factory_info();
  // Using base::Unretained is safe here, unless the browser is being shut down
  // before the Initialize task can be executed. The task is only created as
  // part of class initialization.
  if (io_task_runner_->BelongsToCurrentThread()) {
    InitializeOnIOThread();
    return;
  }
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyIOData::InitializeOnIOThread,
                                base::Unretained(this)));
}

void DataReductionProxyIOData::InitializeOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(network_properties_manager_);

  DCHECK(url_loader_factory_info_);
  auto url_loader_factory = network::SharedURLLoaderFactory::Create(
      std::move(url_loader_factory_info_));

  config_->InitializeOnIOThread(basic_url_request_context_getter_.get(),
                                url_loader_factory,
                                network_properties_manager_.get());
  bypass_stats_->InitializeOnIOThread();
  proxy_delegate_->InitializeOnIOThread(this);
  if (config_client_)
    config_client_->InitializeOnIOThread(url_loader_factory);
  if (ui_task_runner_->BelongsToCurrentThread()) {
    service_->SetIOData(weak_factory_.GetWeakPtr());
    return;
  }
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyService::SetIOData, service_,
                                weak_factory_.GetWeakPtr()));
}

bool DataReductionProxyIOData::IsEnabled() const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return enabled_;
}

void DataReductionProxyIOData::SetPingbackReportingFraction(
    float pingback_reporting_fraction) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataReductionProxyService::SetPingbackReportingFraction,
                     service_, pingback_reporting_fraction));
}

void DataReductionProxyIOData::DeleteBrowsingHistory(const base::Time start,
                                                     const base::Time end) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  network_properties_manager_->DeleteHistory();
}

void DataReductionProxyIOData::OnCacheCleared(const base::Time start,
                                              const base::Time end) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  network_properties_manager_->DeleteHistory();
}

std::unique_ptr<net::URLRequestInterceptor>
DataReductionProxyIOData::CreateInterceptor() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return std::make_unique<DataReductionProxyInterceptor>(
      config_.get(), config_client_.get(), bypass_stats_.get(),
      event_creator_.get());
}

std::unique_ptr<DataReductionProxyNetworkDelegate>
DataReductionProxyIOData::CreateNetworkDelegate(
    std::unique_ptr<net::NetworkDelegate> wrapped_network_delegate,
    bool track_proxy_bypass_statistics) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  std::unique_ptr<DataReductionProxyNetworkDelegate> network_delegate(
      new DataReductionProxyNetworkDelegate(
          std::move(wrapped_network_delegate), config_.get(),
          request_options_.get(), configurator_.get()));
  if (track_proxy_bypass_statistics)
    network_delegate->InitIODataAndUMA(this, bypass_stats_.get());

  return network_delegate;
}

std::unique_ptr<DataReductionProxyDelegate>
DataReductionProxyIOData::CreateProxyDelegate() const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return std::make_unique<DataReductionProxyDelegate>(
      config_.get(), configurator_.get(), event_creator_.get(),
      bypass_stats_.get(), net_log_);
}

// TODO(kundaji): Rename this method to something more descriptive.
// Bug http://crbug/488190.
void DataReductionProxyIOData::SetProxyPrefs(bool enabled, bool at_startup) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // TODO(crbug.com/721403): DRP is disabled with network service enabled. When
  // DRP is switched to mojo, we won't need URLRequestContext.
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    DCHECK(url_request_context_getter_->GetURLRequestContext()
               ->proxy_resolution_service());
  }
  enabled_ = enabled;
  config_->SetProxyConfig(enabled, at_startup);
  if (config_client_) {
    config_client_->SetEnabled(enabled);
    if (enabled)
      config_client_->RetrieveConfig();
  }

  // If Data Saver is disabled, reset data reduction proxy state.
  if (!enabled) {
    // TODO(crbug.com/721403): Make DRP work with network service.
    if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      net::ProxyResolutionService* proxy_resolution_service =
          url_request_context_getter_->GetURLRequestContext()
              ->proxy_resolution_service();
      proxy_resolution_service->ClearBadProxiesCache();
    }

    bypass_stats_->ClearRequestCounts();
    bypass_stats_->NotifyUnavailabilityIfChanged();
  }
}

void DataReductionProxyIOData::SetDataReductionProxyConfiguration(
    const std::string& serialized_config) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (config_client_)
    config_client_->ApplySerializedConfig(serialized_config);
}

void DataReductionProxyIOData::SetIgnoreLongTermBlackListRules(
    bool ignore_long_term_black_list_rules) {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DataReductionProxyService::SetIgnoreLongTermBlackListRules, service_,
          ignore_long_term_black_list_rules));
}

void DataReductionProxyIOData::UpdateDataUseForHost(int64_t network_bytes,
                                                    int64_t original_bytes,
                                                    const std::string& host) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataReductionProxyService::UpdateDataUseForHost, service_,
                     network_bytes, original_bytes, host));
}

void DataReductionProxyIOData::UpdateContentLengths(
    int64_t data_used,
    int64_t original_size,
    bool data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    const std::string& mime_type,
    bool is_user_traffic,
    data_use_measurement::DataUseUserData::DataUseContentType content_type,
    int32_t service_hash_code) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindRepeating(&DataReductionProxyService::UpdateContentLengths,
                          service_, data_used, original_size,
                          data_reduction_proxy_enabled, request_type, mime_type,
                          is_user_traffic, content_type, service_hash_code));
}

void DataReductionProxyIOData::AddEvent(std::unique_ptr<base::Value> event) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyService::AddEvent, service_,
                                std::move(event)));
}

void DataReductionProxyIOData::AddEnabledEvent(
    std::unique_ptr<base::Value> event,
    bool enabled) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyService::AddEnabledEvent,
                                service_, std::move(event), enabled));
}

void DataReductionProxyIOData::AddEventAndSecureProxyCheckState(
    std::unique_ptr<base::Value> event,
    SecureProxyCheckState state) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DataReductionProxyService::AddEventAndSecureProxyCheckState,
          service_, std::move(event), state));
}

void DataReductionProxyIOData::AddAndSetLastBypassEvent(
    std::unique_ptr<base::Value> event,
    int64_t expiration_ticks) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataReductionProxyService::AddAndSetLastBypassEvent,
                     service_, std::move(event), expiration_ticks));
}

void DataReductionProxyIOData::SetUnreachable(bool unreachable) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyService::SetUnreachable,
                                service_, unreachable));
}

void DataReductionProxyIOData::SetInt64Pref(const std::string& pref_path,
                                            int64_t value) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyService::SetInt64Pref,
                                service_, pref_path, value));
}

void DataReductionProxyIOData::SetStringPref(const std::string& pref_path,
                                             const std::string& value) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyService::SetStringPref,
                                service_, pref_path, value));
}

void DataReductionProxyIOData::StoreSerializedConfig(
    const std::string& serialized_config) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  SetStringPref(prefs::kDataReductionProxyConfig, serialized_config);
  SetInt64Pref(prefs::kDataReductionProxyLastConfigRetrievalTime,
               (base::Time::Now() - base::Time()).InMicroseconds());
}

void DataReductionProxyIOData::SetDataUseAscriber(
    data_use_measurement::DataUseAscriber* data_use_ascriber) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(data_use_ascriber);
  data_use_observer_.reset(
      new DataReductionProxyDataUseObserver(this, data_use_ascriber));

  // Disable data use ascriber when data saver is not enabled.
  if (!IsEnabled()) {
    data_use_ascriber->DisableAscriber();
  }
}

void DataReductionProxyIOData::UpdateProxyRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataReductionProxyService::SetProxyRequestHeadersOnUI,
                     service_, std::move(headers)));
  OnProxyConfigUpdated();
}

void DataReductionProxyIOData::OnProxyConfigUpdated() {
  if (!proxy_config_client_)
    return;

  auto config = network::mojom::CustomProxyConfig::New();
  config->rules = configurator_->GetProxyConfig().proxy_rules();

  net::EffectiveConnectionType type = GetEffectiveConnectionType();
  if (type > net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
    DCHECK_NE(net::EFFECTIVE_CONNECTION_TYPE_LAST, type);
    config->pre_cache_headers.SetHeader(
        chrome_proxy_ect_header(),
        net::GetNameForEffectiveConnectionType(type));
  }

  request_options_->AddRequestHeader(&config->post_cache_headers,
                                     base::nullopt);
  proxy_config_client_->OnCustomProxyConfigUpdated(std::move(config));
}

void DataReductionProxyIOData::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  effective_connection_type_ = type;
  OnProxyConfigUpdated();
}

void DataReductionProxyIOData::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  config_->OnRTTOrThroughputEstimatesComputed(http_rtt);
}

net::EffectiveConnectionType
DataReductionProxyIOData::GetEffectiveConnectionType() const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return effective_connection_type_;
}

void DataReductionProxyIOData::SetCustomProxyConfigClient(
    network::mojom::CustomProxyConfigClientPtrInfo config_client_info) {
  proxy_config_client_.Bind(std::move(config_client_info));
  OnProxyConfigUpdated();
}

}  // namespace data_reduction_proxy
