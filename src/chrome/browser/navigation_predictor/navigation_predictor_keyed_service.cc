// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"

#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

NavigationPredictorKeyedService::Prediction::Prediction(
    const content::WebContents* web_contents,
    const base::Optional<GURL>& source_document_url,
    const base::Optional<std::vector<std::string>>& external_app_packages_name,
    PredictionSource prediction_source,
    const std::vector<GURL>& sorted_predicted_urls)
    : web_contents_(web_contents),
      source_document_url_(source_document_url),
      external_app_packages_name_(external_app_packages_name),
      prediction_source_(prediction_source),
      sorted_predicted_urls_(sorted_predicted_urls) {
  switch (prediction_source_) {
    case PredictionSource::kAnchorElementsParsedFromWebPage:
      DCHECK(!source_document_url->is_empty());
      DCHECK(!external_app_packages_name);
      break;
    case PredictionSource::kExternalAndroidApp:
      DCHECK(!web_contents_);
      DCHECK(!source_document_url);
      DCHECK(!external_app_packages_name->empty());
      break;
  }
}

NavigationPredictorKeyedService::Prediction::Prediction(
    const NavigationPredictorKeyedService::Prediction& other)
    : web_contents_(other.web_contents_),
      prediction_source_(other.prediction_source_) {
  // Use non-default copy constructor operator that does deep-copy.
  if (other.source_document_url_)
    source_document_url_ = other.source_document_url_;

  if (other.external_app_packages_name_) {
    external_app_packages_name_ = std::vector<std::string>();
    external_app_packages_name_->reserve(
        other.external_app_packages_name_->size());
    for (const auto& entry : other.external_app_packages_name_.value())
      external_app_packages_name_->push_back(entry);
  }

  sorted_predicted_urls_.reserve(other.sorted_predicted_urls_.size());
  for (const auto& entry : other.sorted_predicted_urls_)
    sorted_predicted_urls_.push_back(entry);
}

NavigationPredictorKeyedService::Prediction&
NavigationPredictorKeyedService::Prediction::operator=(
    const NavigationPredictorKeyedService::Prediction& other) {
  // Use non-default assignment operator that does deep-copy.
  web_contents_ = other.web_contents_;

  source_document_url_.reset();
  if (other.source_document_url_)
    source_document_url_ = other.source_document_url_;

  if (external_app_packages_name_)
    external_app_packages_name_.reset();

  if (other.external_app_packages_name_) {
    external_app_packages_name_ = std::vector<std::string>();
    external_app_packages_name_->reserve(
        other.external_app_packages_name_->size());
    for (const auto& entry : other.external_app_packages_name_.value())
      external_app_packages_name_->push_back(entry);
  } else {
    external_app_packages_name_.reset();
  }
  prediction_source_ = other.prediction_source_;

  sorted_predicted_urls_.clear();
  sorted_predicted_urls_.reserve(other.sorted_predicted_urls_.size());
  for (const auto& entry : other.sorted_predicted_urls_)
    sorted_predicted_urls_.push_back(entry);

  return *this;
}

NavigationPredictorKeyedService::Prediction::~Prediction() = default;

const base::Optional<GURL>&
NavigationPredictorKeyedService::Prediction::source_document_url() const {
  DCHECK_EQ(PredictionSource::kAnchorElementsParsedFromWebPage,
            prediction_source_);
  return source_document_url_;
}

const base::Optional<std::vector<std::string>>&
NavigationPredictorKeyedService::Prediction::external_app_packages_name()
    const {
  DCHECK_EQ(PredictionSource::kExternalAndroidApp, prediction_source_);
  return external_app_packages_name_;
}

const std::vector<GURL>&
NavigationPredictorKeyedService::Prediction::sorted_predicted_urls() const {
  return sorted_predicted_urls_;
}

const content::WebContents*
NavigationPredictorKeyedService::Prediction::web_contents() const {
  DCHECK_EQ(PredictionSource::kAnchorElementsParsedFromWebPage,
            prediction_source_);
  return web_contents_;
}

NavigationPredictorKeyedService::NavigationPredictorKeyedService(
    content::BrowserContext* browser_context)
    : search_engine_preconnector_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!browser_context->IsOffTheRecord());

#if !defined(OS_ANDROID)
  // Start preconnecting to the search engine.
  search_engine_preconnector_.StartPreconnecting(/*with_startup_delay=*/true);
#endif
}

NavigationPredictorKeyedService::~NavigationPredictorKeyedService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void NavigationPredictorKeyedService::OnPredictionUpdated(
    const content::WebContents* web_contents,
    const GURL& document_url,
    PredictionSource prediction_source,
    const std::vector<GURL>& sorted_predicted_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Currently, this method is called only for anchor elements parsed from
  // webpage.
  DCHECK_EQ(PredictionSource::kAnchorElementsParsedFromWebPage,
            prediction_source);

  last_prediction_ = Prediction(web_contents, document_url,
                                /*external_app_packages_name=*/base::nullopt,
                                prediction_source, sorted_predicted_urls);
  for (auto& observer : observer_list_) {
    observer.OnPredictionUpdated(last_prediction_);
  }
}

void NavigationPredictorKeyedService::OnPredictionUpdatedByExternalAndroidApp(
    const std::vector<std::string>& external_app_packages_name,
    const std::vector<GURL>& sorted_predicted_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (external_app_packages_name.empty() || sorted_predicted_urls.empty()) {
    return;
  }
  last_prediction_ =
      Prediction(nullptr, base::nullopt, external_app_packages_name,
                 PredictionSource::kExternalAndroidApp, sorted_predicted_urls);
  for (auto& observer : observer_list_) {
    observer.OnPredictionUpdated(last_prediction_);
  }

  LOCAL_HISTOGRAM_COUNTS_100(
      "NavigationPredictor.ExternalAndroidApp.CountPredictedURLs",
      sorted_predicted_urls.size());

  // TODO(https://crbug.com/1014210): Notify the predicted URLs to the
  // observers.
}

void NavigationPredictorKeyedService::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.AddObserver(observer);
  if (last_prediction_.has_value()) {
    observer->OnPredictionUpdated(last_prediction_);
  }
}

void NavigationPredictorKeyedService::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

SearchEnginePreconnector*
NavigationPredictorKeyedService::search_engine_preconnector() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return &search_engine_preconnector_;
}
