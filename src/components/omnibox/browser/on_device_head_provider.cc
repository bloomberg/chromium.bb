// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_head_provider.h"

#include <limits>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"

namespace {
const int kBaseRelevance = 99;
const size_t kMaxRequestId = std::numeric_limits<size_t>::max() - 1;

bool IsDefaultSearchProviderGoogle(
    const TemplateURLService* template_url_service) {
  if (!template_url_service)
    return false;

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  return default_provider &&
         default_provider->GetEngineType(
             template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;
}

}  // namespace

struct OnDeviceHeadProvider::OnDeviceHeadProviderParams {
  // The id assigned during request creation, which is used to trace this
  // request and determine whether it is current or obsolete.
  const size_t request_id;

  // AutocompleteInput provided by OnDeviceHeadProvider::Start.
  AutocompleteInput input;

  // The suggestions fetched from the on device model which matches the input.
  std::vector<std::string> suggestions;

  // Indicates whether this request failed or not.
  bool failed = false;

  OnDeviceHeadProviderParams(size_t request_id, const AutocompleteInput& input)
      : request_id(request_id), input(input) {}

  ~OnDeviceHeadProviderParams() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(OnDeviceHeadProviderParams);
};

// static
OnDeviceHeadProvider* OnDeviceHeadProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  return new OnDeviceHeadProvider(client, listener);
}

OnDeviceHeadProvider::OnDeviceHeadProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_ON_DEVICE_HEAD),
      client_(client),
      listener_(listener),
      serving_(nullptr),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      on_device_search_request_id_(0),
      weak_ptr_factory_(this) {}

OnDeviceHeadProvider::~OnDeviceHeadProvider() {
  serving_.reset();
}

bool OnDeviceHeadProvider::IsOnDeviceHeadProviderAllowed(
    const AutocompleteInput& input) {
  // Only accept asynchronous request.
  if (!input.want_asynchronous_matches() ||
      input.type() == metrics::OmniboxInputType::INVALID)
    return false;

  // Make sure search suggest is enabled and user is not in incognito.
  if (client()->IsOffTheRecord() || !client()->SearchSuggestEnabled())
    return false;

  // Do not proceed if default search provider is not Google.
  return IsDefaultSearchProviderGoogle(client()->GetTemplateURLService());
}

void OnDeviceHeadProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  TRACE_EVENT0("omnibox", "OnDeviceHeadProvider::Start");

  // Cancel any in-progress request.
  Stop(!minimal_changes, false);

  if (!IsOnDeviceHeadProviderAllowed(input)) {
    matches_.clear();
    return;
  }

  // If the input text has not changed, the result can be reused.
  if (minimal_changes)
    return;

  matches_.clear();
  if (!input.text().empty() && serving_) {
    done_ = false;
    // Note |on_device_search_request_id_| has already been changed in |Stop|
    // so we don't need to change it again here to get a new id for this
    // request.
    std::unique_ptr<OnDeviceHeadProviderParams> params = base::WrapUnique(
        new OnDeviceHeadProviderParams(on_device_search_request_id_, input));
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OnDeviceHeadProvider::DoSearch,
                       weak_ptr_factory_.GetWeakPtr(), std::move(params)));
  }
}

void OnDeviceHeadProvider::Stop(bool clear_cached_results,
                                bool due_to_user_inactivity) {
  // Increase the request_id so that any in-progress requests will become
  // obsolete.
  on_device_search_request_id_ =
      (on_device_search_request_id_ + 1) % kMaxRequestId;

  if (clear_cached_results)
    matches_.clear();

  done_ = true;
}

bool OnDeviceHeadProvider::CreateOnDeviceHeadServingInstance() {
  // TODO(crbug.com/925072): A placeholder later will be used to create
  // serving instance from downloaded model.
  return serving_ ? true : false;
}

void OnDeviceHeadProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(metrics::OmniboxEventProto::ON_DEVICE_HEAD);
  new_entry.set_provider_done(done_);
}

void OnDeviceHeadProvider::DoSearch(
    std::unique_ptr<OnDeviceHeadProviderParams> params) {
  if (serving_ && params &&
      params->request_id == on_device_search_request_id_) {
    // TODO(crbug.com/925072): Add model search time to UMA.
    base::string16 trimmed_input;
    base::TrimWhitespace(params->input.text(), base::TRIM_ALL, &trimmed_input);
    auto results = serving_->GetSuggestionsForPrefix(
        base::UTF16ToUTF8(base::i18n::ToLower(trimmed_input)));
    params->suggestions.clear();
    for (const auto& item : results) {
      // The second member is the score which is not useful for provider.
      params->suggestions.push_back(item.first);
    }
  } else {
    params->failed = true;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceHeadProvider::SearchDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params)));
}

void OnDeviceHeadProvider::SearchDone(
    std::unique_ptr<OnDeviceHeadProviderParams> params) {
  TRACE_EVENT0("omnibox", "OnDeviceHeadProvider::SearchDone");
  // Ignore this request if it has been stopped or a new one has already been
  // created.
  if (!params || params->request_id != on_device_search_request_id_)
    return;

  const TemplateURLService* template_url_service =
      client()->GetTemplateURLService();

  if (IsDefaultSearchProviderGoogle(template_url_service) && !params->failed) {
    matches_.clear();
    int relevance = kBaseRelevance;
    for (const auto& item : params->suggestions) {
      matches_.push_back(BaseSearchProvider::CreateOnDeviceSearchSuggestion(
          /*autocomplete_provider=*/this, /*input=*/params->input,
          /*suggestion=*/base::UTF8ToUTF16(item), /*relevance=*/relevance--,
          /*template_url=*/
          template_url_service->GetDefaultSearchProvider(),
          /*search_terms_data=*/
          template_url_service->search_terms_data(),
          /*accepted_suggestion=*/TemplateURLRef::NO_SUGGESTION_CHOSEN));
    }
  }

  done_ = true;
  listener_->OnProviderUpdate(true);
}
