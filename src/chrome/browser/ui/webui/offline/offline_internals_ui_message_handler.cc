// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/offline/offline_internals_ui_message_handler.h"

#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "content/public/browser/web_ui.h"
#include "net/base/network_change_notifier.h"

namespace offline_internals {

namespace {

std::string GetStringFromDeletePageResult(
    offline_pages::DeletePageResult value) {
  switch (value) {
    case offline_pages::DeletePageResult::SUCCESS:
      return "Success";
    case offline_pages::DeletePageResult::CANCELLED:
      return "Cancelled";
    case offline_pages::DeletePageResult::STORE_FAILURE:
      return "Store failure";
    case offline_pages::DeletePageResult::DEVICE_FAILURE:
      return "Device failure";
    case offline_pages::DeletePageResult::DEPRECATED_NOT_FOUND:
      return "Not found";
  }
  NOTREACHED();
  return "Unknown";
}

std::string GetStringFromDeleteRequestResults(
    const offline_pages::MultipleItemStatuses& results) {
  // If any requests failed, return "failure", else "success".
  for (const auto& result : results) {
    if (result.second == offline_pages::ItemActionStatus::STORE_ERROR)
      return "Store failure, could not delete one or more requests";
  }

  return "Success";
}

std::string GetStringFromSavePageStatus() {
  return "Available";
}

}  // namespace

OfflineInternalsUIMessageHandler::OfflineInternalsUIMessageHandler()
    : offline_page_model_(nullptr),
      request_coordinator_(nullptr),
      prefetch_service_(nullptr) {}

OfflineInternalsUIMessageHandler::~OfflineInternalsUIMessageHandler() {}

void OfflineInternalsUIMessageHandler::HandleDeleteSelectedPages(
    const base::ListValue* args) {
  base::Value::ConstListView args_list = args->GetList();
  CHECK_EQ(2u, args_list.size());
  std::string callback_id = args_list[0].GetString();

  std::vector<int64_t> offline_ids;
  base::Value::ConstListView offline_ids_from_arg = args_list[1].GetList();
  for (size_t i = 0; i < offline_ids_from_arg.size(); i++) {
    std::string value = offline_ids_from_arg[i].GetString();
    int64_t int_value;
    base::StringToInt64(value, &int_value);
    offline_ids.push_back(int_value);
  }

  offline_pages::PageCriteria criteria;
  criteria.offline_ids = std::move(offline_ids);
  offline_page_model_->DeletePagesWithCriteria(
      criteria,
      base::BindOnce(
          &OfflineInternalsUIMessageHandler::HandleDeletedPagesCallback,
          weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void OfflineInternalsUIMessageHandler::HandleDeleteSelectedRequests(
    const base::ListValue* args) {
  base::Value::ConstListView args_list = args->GetList();
  CHECK_EQ(2u, args_list.size());
  std::string callback_id = args_list[0].GetString();

  std::vector<int64_t> offline_ids;
  base::Value::ConstListView offline_ids_from_arg = args_list[1].GetList();
  for (size_t i = 0; i < offline_ids_from_arg.size(); i++) {
    std::string value = offline_ids_from_arg[i].GetString();
    int64_t int_value;
    base::StringToInt64(value, &int_value);
    offline_ids.push_back(int_value);
  }

  // Call RequestCoordinator to delete them
  if (request_coordinator_) {
    request_coordinator_->RemoveRequests(
        offline_ids,
        base::BindOnce(
            &OfflineInternalsUIMessageHandler::HandleDeletedRequestsCallback,
            weak_ptr_factory_.GetWeakPtr(), callback_id));
  }
}

void OfflineInternalsUIMessageHandler::HandleDeletedPagesCallback(
    std::string callback_id,
    offline_pages::DeletePageResult result) {
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(GetStringFromDeletePageResult(result)));
}

void OfflineInternalsUIMessageHandler::HandleDeletedRequestsCallback(
    std::string callback_id,
    const offline_pages::MultipleItemStatuses& results) {
  ResolveJavascriptCallback(
      base::Value(callback_id),
      base::Value(GetStringFromDeleteRequestResults(results)));
}

void OfflineInternalsUIMessageHandler::HandleStoredPagesCallback(
    std::string callback_id,
    const offline_pages::MultipleOfflinePageItemResult& pages) {
  std::vector<base::Value> results;
  for (const auto& page : pages) {
    base::Value offline_page(base::Value::Type::DICTIONARY);
    offline_page.SetStringKey("onlineUrl", page.url.spec());
    offline_page.SetStringKey("namespace", page.client_id.name_space);
    offline_page.SetDoubleKey("size", page.file_size);
    offline_page.SetStringKey("id", std::to_string(page.offline_id));
    offline_page.SetStringKey("filePath", page.file_path.MaybeAsASCII());
    offline_page.SetDoubleKey("creationTime", page.creation_time.ToJsTime());
    offline_page.SetDoubleKey("lastAccessTime",
                              page.last_access_time.ToJsTime());
    offline_page.SetIntKey("accessCount", page.access_count);
    offline_page.SetStringKey("originalUrl",
                              page.original_url_if_different.spec());
    offline_page.SetStringKey("requestOrigin", page.request_origin);
    results.push_back(std::move(offline_page));
  }
  // Sort by creation order.
  std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
    return a.FindKey({"creationTime"})->GetDouble() <
           b.FindKey({"creationTime"})->GetDouble();
  });

  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(std::move(results)));
}

void OfflineInternalsUIMessageHandler::HandleRequestQueueCallback(
    std::string callback_id,
    std::vector<std::unique_ptr<offline_pages::SavePageRequest>> requests) {
  base::ListValue save_page_requests;
  for (const auto& request : requests) {
    auto save_page_request = std::make_unique<base::DictionaryValue>();
    save_page_request->SetString("onlineUrl", request->url().spec());
    save_page_request->SetDoubleKey("creationTime",
                                    request->creation_time().ToJsTime());
    save_page_request->SetString("status", GetStringFromSavePageStatus());
    save_page_request->SetString("namespace", request->client_id().name_space);
    save_page_request->SetDoubleKey("lastAttemptTime",
                                    request->last_attempt_time().ToJsTime());
    save_page_request->SetString("id", std::to_string(request->request_id()));
    save_page_request->SetString("originalUrl", request->original_url().spec());
    save_page_request->SetString("requestOrigin", request->request_origin());
    save_page_requests.Append(std::move(save_page_request));
  }
  ResolveJavascriptCallback(base::Value(callback_id), save_page_requests);
}

void OfflineInternalsUIMessageHandler::HandleGetRequestQueue(
    const base::ListValue* args) {
  AllowJavascript();
  const std::string& callback_id = args->GetList()[0].GetString();

  if (request_coordinator_) {
    request_coordinator_->GetAllRequests(base::BindOnce(
        &OfflineInternalsUIMessageHandler::HandleRequestQueueCallback,
        weak_ptr_factory_.GetWeakPtr(), callback_id));
  } else {
    base::ListValue results;
    ResolveJavascriptCallback(base::Value(callback_id), results);
  }
}

void OfflineInternalsUIMessageHandler::HandleGetStoredPages(
    const base::ListValue* args) {
  AllowJavascript();
  const std::string& callback_id = args->GetList()[0].GetString();

  if (offline_page_model_) {
    offline_page_model_->GetAllPages(base::BindOnce(
        &OfflineInternalsUIMessageHandler::HandleStoredPagesCallback,
        weak_ptr_factory_.GetWeakPtr(), callback_id));
  } else {
    base::ListValue results;
    ResolveJavascriptCallback(base::Value(callback_id), results);
  }
}

void OfflineInternalsUIMessageHandler::HandleSetRecordPageModel(
    const base::ListValue* args) {
  AllowJavascript();
  const auto& list = args->GetList();
  CHECK(!list.empty());
  const bool should_record = list[0].GetBool();
  if (offline_page_model_)
    offline_page_model_->GetLogger()->SetIsLogging(should_record);
}

void OfflineInternalsUIMessageHandler::HandleGetNetworkStatus(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  ResolveJavascriptCallback(
      callback_id,
      base::Value(net::NetworkChangeNotifier::IsOffline() ? "Offline"
                                                          : "Online"));
}

void OfflineInternalsUIMessageHandler::HandleScheduleNwake(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  if (prefetch_service_) {
    prefetch_service_->ForceRefreshSuggestions();
    prefetch_service_->GetPrefetchBackgroundTaskHandler()
        ->EnsureTaskScheduled();
    ResolveJavascriptCallback(callback_id, base::Value("Scheduled."));
  } else {
    RejectJavascriptCallback(callback_id,
                             base::Value("No prefetch service available."));
  }
}

void OfflineInternalsUIMessageHandler::HandleCancelNwake(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  if (prefetch_service_) {
    prefetch_service_->GetPrefetchBackgroundTaskHandler()
        ->CancelBackgroundTask();
    ResolveJavascriptCallback(callback_id, base::Value("Cancelled."));
  } else {
    RejectJavascriptCallback(callback_id,
                             base::Value("No prefetch service available."));
  }
}

void OfflineInternalsUIMessageHandler::HandleGeneratePageBundle(
    const base::ListValue* args) {
  AllowJavascript();
  const std::string& callback_id = args->GetList()[0].GetString();

  if (!prefetch_service_) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value("No prefetch service available."));
    return;
  }

  const std::string& data = args->GetList()[1].GetString();
  std::vector<std::string> page_urls = base::SplitStringUsingSubstr(
      data, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<offline_pages::PrefetchURL> prefetch_urls;
  for (auto& page_url : page_urls) {
    // Creates a dummy prefetch URL with a bogus ID, and using the URL as the
    // page title.
    GURL url(page_url);
    if (url.is_valid()) {
      prefetch_urls.push_back(offline_pages::PrefetchURL(
          "offline-internals", url, base::UTF8ToUTF16(page_url)));
    }
  }

  prefetch_service_->GetPrefetchDispatcher()->AddCandidatePrefetchURLs(
      offline_pages::kSuggestedArticlesNamespace, prefetch_urls);
  // Note: Static casts are needed here so that both Windows and Android can
  // compile these printf formats.
  std::string message =
      base::StringPrintf("Added %zu candidate URLs.", prefetch_urls.size());
  if (prefetch_urls.size() < page_urls.size()) {
    size_t invalid_urls_count = page_urls.size() - prefetch_urls.size();
    message.append(
        base::StringPrintf(" Ignored %zu invalid URLs.", invalid_urls_count));
  }
  message.append("\n");

  // Construct a JSON array containing all the URLs. To guard against malicious
  // URLs that might contain special characters, we create a ListValue and then
  // serialize it into JSON, instead of doing direct string manipulation.
  base::ListValue urls;
  for (const auto& prefetch_url : prefetch_urls) {
    urls.Append(prefetch_url.url.spec());
  }
  std::string json;
  base::JSONWriter::Write(urls, &json);
  message.append(json);
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(message));
}

void OfflineInternalsUIMessageHandler::HandleGetOperation(
    const base::ListValue* args) {
  AllowJavascript();
  const std::string& callback_id = args->GetList()[0].GetString();

  if (!prefetch_service_) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value("No prefetch service available."));
    return;
  }

  std::string name = args->GetList()[1].GetString();
  base::TrimWhitespaceASCII(name, base::TRIM_ALL, &name);

  prefetch_service_->GetPrefetchDispatcher()
      ->GCMOperationCompletedMessageReceived(name);
  base::Value message("GetOperation will be attempted for any matching pages.");
  ResolveJavascriptCallback(base::Value(callback_id), message);
}

void OfflineInternalsUIMessageHandler::HandleDownloadArchive(
    const base::ListValue* args) {
  AllowJavascript();
  std::string name = args->GetList()[0].GetString();
  base::TrimWhitespaceASCII(name, base::TRIM_ALL, &name);

  if (prefetch_service_) {
    prefetch_service_->GetPrefetchDownloader()->StartDownload(
        base::GenerateGUID(), name, std::string());
  }
}

void OfflineInternalsUIMessageHandler::HandleSetRecordRequestQueue(
    const base::ListValue* args) {
  AllowJavascript();
  const auto& list = args->GetList();
  CHECK(!list.empty());
  const bool should_record = list[0].GetBool();
  if (request_coordinator_)
    request_coordinator_->GetLogger()->SetIsLogging(should_record);
}

void OfflineInternalsUIMessageHandler::HandleSetRecordPrefetchService(
    const base::ListValue* args) {
  AllowJavascript();
  const auto& list = args->GetList();
  CHECK(!list.empty());
  const bool should_record = list[0].GetBool();
  if (prefetch_service_)
    prefetch_service_->GetLogger()->SetIsLogging(should_record);
}

void OfflineInternalsUIMessageHandler::HandleSetLimitlessPrefetchingEnabled(
    const base::ListValue* args) {
  AllowJavascript();
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  DCHECK(!args->GetList().empty());
  bool enabled = args->GetList()[0].GetBool();
  offline_pages::prefetch_prefs::SetLimitlessPrefetchingEnabled(prefs, enabled);
}

void OfflineInternalsUIMessageHandler::HandleGetLimitlessPrefetchingEnabled(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  bool enabled =
      offline_pages::prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs);

  ResolveJavascriptCallback(callback_id, base::Value(enabled));
}

void OfflineInternalsUIMessageHandler::HandleSetPrefetchTestingHeader(
    const base::ListValue* args) {
  AllowJavascript();
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();

  if (args->GetList().size() != 1) {
    DLOG(ERROR) << "Expected 1 argument to setPrefetchTesting header but got "
                << args->GetList().size();
    return;
  }
  if (!args->GetList()[0].is_string()) {
    DLOG(ERROR) << "Expected argument to be string but got "
                << base::Value::GetTypeName(args->GetList()[0].type());
    return;
  }

  offline_pages::prefetch_prefs::SetPrefetchTestingHeader(
      prefs, args->GetList()[0].GetString());

  if (prefetch_service_)
    prefetch_service_->SetEnabledByServer(prefs, true);
}

void OfflineInternalsUIMessageHandler::HandleGetPrefetchTestingHeader(
    const base::ListValue* args) {
  AllowJavascript();
  if (args->GetList().size() != 1) {
    DLOG(ERROR) << "Expected 1 argument to getPrefetchTestingHeader but got "
                << args->GetList().size();
    return;
  }
  if (!args->GetList()[0].is_string()) {
    DLOG(ERROR) << "Expected callback_id to be a string but got "
                << base::Value::GetTypeName(args->GetList()[0].type());
    return;
  }

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  ResolveJavascriptCallback(
      args->GetList()[0],
      base::Value(offline_pages::prefetch_prefs::GetPrefetchTestingHeader(prefs)

                      ));
}

void OfflineInternalsUIMessageHandler::HandleGetLoggingState(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  base::DictionaryValue result;
  result.SetBoolean("modelIsLogging",
                    offline_page_model_
                        ? offline_page_model_->GetLogger()->GetIsLogging()
                        : false);
  result.SetBoolean("queueIsLogging",
                    request_coordinator_
                        ? request_coordinator_->GetLogger()->GetIsLogging()
                        : false);
  bool prefetch_logging = false;
  if (prefetch_service_) {
    prefetch_logging = prefetch_service_->GetLogger()->GetIsLogging();
  }
  result.SetBoolean("prefetchIsLogging", prefetch_logging);
  ResolveJavascriptCallback(callback_id, result);
}

void OfflineInternalsUIMessageHandler::HandleGetEventLogs(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  std::vector<std::string> logs;
  if (offline_page_model_)
    offline_page_model_->GetLogger()->GetLogs(&logs);
  if (request_coordinator_)
    request_coordinator_->GetLogger()->GetLogs(&logs);
  if (prefetch_service_)
    prefetch_service_->GetLogger()->GetLogs(&logs);
  std::sort(logs.begin(), logs.end());

  base::ListValue result;
  for (const std::string& log : logs) {
    result.Append(log);
  }

  ResolveJavascriptCallback(callback_id, result);
}

void OfflineInternalsUIMessageHandler::HandleAddToRequestQueue(
    const base::ListValue* args) {
  const std::string& callback_id = args->GetList()[0].GetString();

  if (request_coordinator_) {
    const std::string& url = args->GetList()[1].GetString();

    // To be visible in Downloads UI, these items need a well-formed GUID
    // and AsyncNamespace in their ClientId.
    std::ostringstream id_stream;
    id_stream << base::GenerateGUID();

    offline_pages::RequestCoordinator::SavePageLaterParams params;
    params.url = GURL(url);
    params.client_id = offline_pages::ClientId(offline_pages::kAsyncNamespace,
                                               id_stream.str());
    request_coordinator_->SavePageLater(
        params,
        base::BindOnce(
            &OfflineInternalsUIMessageHandler::HandleSavePageLaterCallback,
            weak_ptr_factory_.GetWeakPtr(), callback_id));
  } else {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value(false));
  }
}

void OfflineInternalsUIMessageHandler::HandleSavePageLaterCallback(
    std::string callback_id,
    offline_pages::AddRequestResult result) {
  ResolveJavascriptCallback(
      base::Value(callback_id),
      base::Value(result == offline_pages::AddRequestResult::SUCCESS));
}

void OfflineInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "deleteSelectedPages",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleDeleteSelectedPages,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "deleteSelectedRequests",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleDeleteSelectedRequests,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getRequestQueue",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleGetRequestQueue,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getStoredPages",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleGetStoredPages,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getEventLogs",
      base::BindRepeating(&OfflineInternalsUIMessageHandler::HandleGetEventLogs,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "setRecordRequestQueue",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleSetRecordRequestQueue,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "setRecordPageModel",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleSetRecordPageModel,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "setRecordPrefetchService",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleSetRecordPrefetchService,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "setLimitlessPrefetchingEnabled",
      base::BindRepeating(&OfflineInternalsUIMessageHandler::
                              HandleSetLimitlessPrefetchingEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getLimitlessPrefetchingEnabled",
      base::BindRepeating(&OfflineInternalsUIMessageHandler::
                              HandleGetLimitlessPrefetchingEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "setPrefetchTestingHeader",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleSetPrefetchTestingHeader,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getPrefetchTestingHeader",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleGetPrefetchTestingHeader,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getLoggingState",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleGetLoggingState,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "addToRequestQueue",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleAddToRequestQueue,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getNetworkStatus",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleGetNetworkStatus,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "scheduleNwake",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleScheduleNwake,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "cancelNwake",
      base::BindRepeating(&OfflineInternalsUIMessageHandler::HandleCancelNwake,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "generatePageBundle",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleGeneratePageBundle,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getOperation",
      base::BindRepeating(&OfflineInternalsUIMessageHandler::HandleGetOperation,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "downloadArchive",
      base::BindRepeating(
          &OfflineInternalsUIMessageHandler::HandleDownloadArchive,
          base::Unretained(this)));

  // Get the offline page model associated with this web ui.
  Profile* profile = Profile::FromWebUI(web_ui());
  offline_page_model_ =
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(profile);
  request_coordinator_ =
      offline_pages::RequestCoordinatorFactory::GetForBrowserContext(profile);
  prefetch_service_ = offline_pages::PrefetchServiceFactory::GetForKey(
      profile->GetProfileKey());
}

void OfflineInternalsUIMessageHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace offline_internals
