// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/quota_internals/quota_internals_handler.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/quota_internals/quota_internals_proxy.h"
#include "chrome/browser/ui/webui/quota_internals/quota_internals_types.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_features.h"

using content::BrowserContext;

namespace {

bool IsStoragePressureEnabled() {
#if defined(OS_ANDROID)
  return false;
#else
  return true;
#endif
}

}  // namespace

namespace quota_internals {

QuotaInternalsHandler::QuotaInternalsHandler() {}

QuotaInternalsHandler::~QuotaInternalsHandler() {
  if (proxy_.get())
    proxy_->handler_ = nullptr;
}

void QuotaInternalsHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "requestInfo", base::BindRepeating(&QuotaInternalsHandler::OnRequestInfo,
                                         base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "triggerStoragePressure",
      base::BindRepeating(&QuotaInternalsHandler::OnTriggerStoragePressure,
                          base::Unretained(this)));
}

void QuotaInternalsHandler::ReportAvailableSpace(int64_t available_space) {
  FireWebUIListener("AvailableSpaceUpdated",
                    base::Value(static_cast<double>(available_space)));
}

void QuotaInternalsHandler::ReportGlobalInfo(const GlobalStorageInfo& data) {
  std::unique_ptr<base::Value> value(data.NewValue());
  FireWebUIListener("GlobalInfoUpdated", *value);
}

void QuotaInternalsHandler::ReportPerHostInfo(
    const std::vector<PerHostStorageInfo>& hosts) {
  base::ListValue values;
  for (auto itr(hosts.begin()); itr != hosts.end(); ++itr) {
    values.Append(itr->NewValue());
  }

  FireWebUIListener("PerHostInfoUpdated", values);
}

void QuotaInternalsHandler::ReportPerOriginInfo(
    const std::vector<PerOriginStorageInfo>& origins) {
  base::ListValue origins_value;
  for (auto itr(origins.begin()); itr != origins.end(); ++itr) {
    origins_value.Append(itr->NewValue());
  }

  FireWebUIListener("PerOriginInfoUpdated", origins_value);
}

void QuotaInternalsHandler::ReportStatistics(const Statistics& stats) {
  base::DictionaryValue dict;
  for (auto itr(stats.begin()); itr != stats.end(); ++itr) {
    dict.SetStringKey(itr->first, itr->second);
  }

  FireWebUIListener("StatisticsUpdated", dict);
}

void QuotaInternalsHandler::ReportStoragePressureFlag() {
  base::DictionaryValue flag_enabled;
  flag_enabled.SetBoolean("isStoragePressureEnabled",
                          IsStoragePressureEnabled());
  FireWebUIListener("StoragePressureFlagUpdated", flag_enabled);
}

void QuotaInternalsHandler::OnRequestInfo(const base::ListValue*) {
  AllowJavascript();
  if (!proxy_.get())
    proxy_ = new QuotaInternalsProxy(this);
  ReportStoragePressureFlag();
  proxy_->RequestInfo(Profile::FromWebUI(web_ui())
                          ->GetDefaultStoragePartition()
                          ->GetQuotaManager());
}

void QuotaInternalsHandler::OnTriggerStoragePressure(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetList().size());
  const std::string& origin_string = args->GetList()[0].GetString();
  GURL url(origin_string);

  if (!proxy_.get())
    proxy_ = new QuotaInternalsProxy(this);
  proxy_->TriggerStoragePressure(url::Origin::Create(url),
                                 Profile::FromWebUI(web_ui())
                                     ->GetDefaultStoragePartition()
                                     ->GetQuotaManager());
}

}  // namespace quota_internals
