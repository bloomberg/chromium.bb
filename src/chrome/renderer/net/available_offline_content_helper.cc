// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/available_offline_content_helper.h"

#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "chrome/common/available_offline_content.mojom.h"
#include "components/error_page/common/net_error_info.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

using chrome::mojom::AvailableOfflineContentPtr;

base::Value AvailableContentToValue(const AvailableOfflineContentPtr& content) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey("ID", base::Value(content->id));
  value.SetKey("name_space", base::Value(content->name_space));
  value.SetKey("title", base::Value(content->title));
  value.SetKey("snippet", base::Value(content->snippet));
  value.SetKey("date_modified", base::Value(content->date_modified));
  value.SetKey("attribution", base::Value(content->attribution));
  value.SetKey("thumbnail_data_uri",
               base::Value(content->thumbnail_data_uri.spec()));
  value.SetKey("content_type",
               base::Value(static_cast<int>(content->content_type)));
  return value;
}

base::Value AvailableContentListToValue(
    const std::vector<AvailableOfflineContentPtr>& content_list) {
  base::Value value(base::Value::Type::LIST);
  for (const auto& content : content_list) {
    value.GetList().push_back(AvailableContentToValue(content));
  }
  return value;
}

base::Value AvailableContentSummaryToValue(
    const chrome::mojom::AvailableOfflineContentSummaryPtr& summary) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey("total_items",
               base::Value(base::saturated_cast<int>(summary->total_items)));
  value.SetKey("has_prefetched_page",
               base::Value(summary->has_prefetched_page));
  value.SetKey("has_offline_page", base::Value(summary->has_offline_page));
  value.SetKey("has_video", base::Value(summary->has_video));
  value.SetKey("has_audio", base::Value(summary->has_audio));
  return value;
}

void RecordSuggestionPresented(
    const std::vector<AvailableOfflineContentPtr>& suggestions) {
  for (const AvailableOfflineContentPtr& item : suggestions) {
    UMA_HISTOGRAM_ENUMERATION("Net.ErrorPageCounts.SuggestionPresented",
                              item->content_type);
  }
}

}  // namespace

AvailableOfflineContentHelper::AvailableOfflineContentHelper() = default;
AvailableOfflineContentHelper::~AvailableOfflineContentHelper() = default;

void AvailableOfflineContentHelper::Reset() {
  provider_.reset();
}

void AvailableOfflineContentHelper::FetchAvailableContent(
    base::OnceCallback<void(const std::string& offline_content_json)>
        callback) {
  if (!BindProvider()) {
    std::move(callback).Run({});
    return;
  }
  provider_->List(
      base::BindOnce(&AvailableOfflineContentHelper::AvailableContentReceived,
                     base::Unretained(this), std::move(callback)));
}

void AvailableOfflineContentHelper::FetchSummary(
    base::OnceCallback<void(const std::string& content_summary_json)>
        callback) {
  if (!BindProvider()) {
    std::move(callback).Run({});
    return;
  }
  provider_->Summarize(
      base::BindOnce(&AvailableOfflineContentHelper::SummaryReceived,
                     base::Unretained(this), std::move(callback)));
}

bool AvailableOfflineContentHelper::BindProvider() {
  if (provider_)
    return true;
  content::RenderThread::Get()->GetConnector()->BindInterface(
      content::mojom::kBrowserServiceName, &provider_);
  return !!provider_;
}

void AvailableOfflineContentHelper::LaunchItem(const std::string& id,
                                               const std::string& name_space) {
  if (!BindProvider())
    return;

  provider_->LaunchItem(id, name_space);

  for (const AvailableOfflineContentPtr& item : fetched_content_) {
    if (item->id == id && item->name_space == name_space) {
      UMA_HISTOGRAM_ENUMERATION("Net.ErrorPageCounts.SuggestionClicked",
                                item->content_type);
      RecordEvent(error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTION_CLICKED);
      return;
    }
  }
  NOTREACHED();
}

void AvailableOfflineContentHelper::LaunchDownloadsPage() {
  if (!BindProvider())
    return;
  RecordEvent(error_page::NETWORK_ERROR_PAGE_OFFLINE_DOWNLOADS_PAGE_CLICKED);
  provider_->LaunchDownloadsPage();
}

void AvailableOfflineContentHelper::AvailableContentReceived(
    base::OnceCallback<void(const std::string& offline_content_json)> callback,
    std::vector<AvailableOfflineContentPtr> content) {
  fetched_content_ = std::move(content);
  std::string json;

  if (!fetched_content_.empty()) {
    RecordSuggestionPresented(fetched_content_);
    RecordEvent(error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN);
    base::JSONWriter::Write(AvailableContentListToValue(fetched_content_),
                            &json);
  }
  std::move(callback).Run(json);
  // We don't need to retain the thumbnail here, so free up some memory.
  for (const AvailableOfflineContentPtr& item : fetched_content_) {
    item->thumbnail_data_uri = GURL();
  }
}

void AvailableOfflineContentHelper::SummaryReceived(
    base::OnceCallback<void(const std::string& content_summary_json)> callback,
    chrome::mojom::AvailableOfflineContentSummaryPtr summary) {
  if (summary->total_items == 0) {
    std::move(callback).Run("");
  } else {
    std::string json;
    base::JSONWriter::Write(AvailableContentSummaryToValue(summary), &json);
    RecordEvent(error_page::NETWORK_ERROR_PAGE_OFFLINE_CONTENT_SUMMARY_SHOWN);
    std::move(callback).Run(json);
  }
}
