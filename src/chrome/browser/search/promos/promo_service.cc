// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/promos/promo_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/google/core/common/google_util.h"
#include "content/public/common/service_manager_connection.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

const char kNewTabPromosApiPath[] = "/async/newtab_promos";

const char kXSSIResponsePreamble[] = ")]}'";

// Parses an update proto from |value|. Will return base::nullopt if |value|
// is not of the form: {"update":{"promos":{"log_url":"","middle":""}}}
// Resolves the log_url against the base_url to form a valid GURL.
base::Optional<PromoData> JsonToPromoData(const base::Value& value,
                                          const GURL& base_url) {
  const base::DictionaryValue* dict = nullptr;
  if (!value.GetAsDictionary(&dict)) {
    DVLOG(1) << "Parse error: top-level dictionary not found";
    return base::nullopt;
  }

  const base::DictionaryValue* update = nullptr;
  if (!dict->GetDictionary("update", &update)) {
    DVLOG(1) << "Parse error: no update";
    return base::nullopt;
  }

  const base::DictionaryValue* promos = nullptr;
  if (!update->GetDictionary("promos", &promos)) {
    DVLOG(1) << "Parse error: no promos";
    return base::nullopt;
  }

  std::string middle = std::string();
  if (!promos->GetString("middle", &middle)) {
    DVLOG(1) << "No middle promo";
    return base::nullopt;
  }

  PromoData result;
  result.promo_html = middle;

  std::string log_url = std::string();
  if (!promos->GetString("log_url", &log_url)) {
    DVLOG(1) << "No promo log_url";
    return base::nullopt;
  }

  GURL promo_log_url = base_url.Resolve(log_url);
  result.promo_log_url = promo_log_url;

  return result;
}

}  // namespace

PromoService::PromoService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GoogleURLTracker* google_url_tracker)
    : url_loader_factory_(url_loader_factory),
      google_url_tracker_(google_url_tracker),
      weak_ptr_factory_(this) {}

PromoService::~PromoService() = default;

GURL PromoService::GetGoogleBaseUrl() const {
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = google_url_tracker_->google_url();
  }

  return google_base_url;
}

GURL PromoService::GetApiUrl() const {
  GURL api_url = GetGoogleBaseUrl().Resolve(kNewTabPromosApiPath);

  return api_url;
}

void PromoService::Refresh() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("promo_service", R"(
        semantics {
          sender: "Promo Service"
          description: "Downloads promos."
          trigger:
            "Displaying the new tab page on Desktop, if Google is the "
            "configured search provider."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature via selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetApiUrl();
  resource_request->load_flags = net::LOAD_DO_NOT_SEND_AUTH_DATA;

  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  simple_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PromoService::OnLoadDone, base::Unretained(this)),
      1024 * 1024);
}

void PromoService::OnLoadDone(std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DVLOG(1) << "Request failed with error: " << simple_loader_->NetError();
    PromoDataLoaded(Status::TRANSIENT_ERROR, base::nullopt);
    return;
  }

  std::string response;
  response.swap(*response_body);

  // The response may start with )]}'. Ignore this.
  if (base::StartsWith(response, kXSSIResponsePreamble,
                       base::CompareCase::SENSITIVE)) {
    response = response.substr(strlen(kXSSIResponsePreamble));
  }

  data_decoder::SafeJsonParser::Parse(
      content::ServiceManagerConnection::GetForProcess()->GetConnector(),
      response,
      base::BindRepeating(&PromoService::OnJsonParsed,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PromoService::OnJsonParseFailed,
                          weak_ptr_factory_.GetWeakPtr()));
}

void PromoService::OnJsonParsed(std::unique_ptr<base::Value> value) {
  const GURL google_base_url = GetGoogleBaseUrl();
  base::Optional<PromoData> result = JsonToPromoData(*value, google_base_url);
  PromoDataLoaded(result.has_value() ? Status::OK : Status::FATAL_ERROR,
                  result);
}

void PromoService::OnJsonParseFailed(const std::string& message) {
  DVLOG(1) << "Parsing JSON failed: " << message;
  PromoDataLoaded(Status::FATAL_ERROR, base::nullopt);
}

void PromoService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnPromoServiceShuttingDown();
  }

  DCHECK(!observers_.might_have_observers());
}

void PromoService::AddObserver(PromoServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void PromoService::RemoveObserver(PromoServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PromoService::PromoDataLoaded(Status status,
                                   const base::Optional<PromoData>& data) {
  // In case of transient errors, keep our cached data (if any), but still
  // notify observers of the finished load (attempt).
  if (status != Status::TRANSIENT_ERROR) {
    promo_data_ = data;
  }
  promo_status_ = status;
  NotifyObservers();
}

void PromoService::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnPromoDataUpdated();
  }
}

GURL PromoService::GetLoadURLForTesting() const {
  return GetApiUrl();
}
