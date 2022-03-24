// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ITEM_SUGGEST_CACHE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ITEM_SUGGEST_CACHE_H_

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class Profile;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace app_list {

class ItemSuggestCache {
 public:
  // Information on a single file suggestion result.
  struct Result {
    Result(const std::string& id, const std::string& title);
    Result(const Result& other);
    ~Result();

    std::string id;
    std::string title;
  };

  // Information on all file suggestion results returned from an ItemSuggest
  // request.
  struct Results {
    explicit Results(const std::string& suggestion_id);
    Results(const Results& other);
    ~Results();

    // |suggestion_id| should be used to link ItemSuggest feedback to a
    // particular request.
    std::string suggestion_id;
    // |results| has the same ordering as the response from ItemSuggest:
    // best-to-worst.
    std::vector<Result> results;
  };

  ItemSuggestCache(
      Profile* profile,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ItemSuggestCache();

  ItemSuggestCache(const ItemSuggestCache&) = delete;
  ItemSuggestCache& operator=(const ItemSuggestCache&) = delete;

  // Returns the results currently in the cache.
  absl::optional<ItemSuggestCache::Results> GetResults();

  // Updates the cache by calling ItemSuggest.
  void UpdateCache();

  static absl::optional<ItemSuggestCache::Results> ConvertJsonForTest(
      const base::Value* value);

  // Whether or not to override configuration of the cache with an experiment.
  static const base::Feature kExperiment;

  // Possible outcomes of a call to the ItemSuggest API. These values persist to
  // logs. Entries should not be renumbered and numeric values should never be
  // reused.
  enum class Status {
    kOk = 0,
    kDisabledByExperiment = 1,
    kDisabledByPolicy = 2,
    kInvalidServerUrl = 3,
    kNoIdentityManager = 4,
    kGoogleAuthError = 5,
    kNetError = 6,
    kResponseTooLarge = 7,
    k3xxStatus = 8,
    k4xxStatus = 9,
    k5xxStatus = 10,
    kEmptyResponse = 11,
    kNoResultsInResponse = 12,
    kJsonParseFailure = 13,
    kJsonConversionFailure = 14,
    kPostLaunchUpdateIgnored = 15,
    kMaxValue = kPostLaunchUpdateIgnored,
  };

 private:
  // Whether or not the ItemSuggestCache is enabled.
  static constexpr base::FeatureParam<bool> kEnabled{&kExperiment, "enabled",
                                                     true};
  // The url of the service that fetches descriptions given image pixels.
  static constexpr base::FeatureParam<std::string> kServerUrl{
      &kExperiment, "server_url",
      "https://appsitemsuggest-pa.googleapis.com/v1/items"};

  // Specifies the ItemSuggest backend that should be used to serve our
  // requests.
  static constexpr base::FeatureParam<std::string> kModelName{
      &kExperiment, "model_name", "quick_access"};

  static constexpr base::FeatureParam<int> kMinMinutesBetweenUpdates{
      &kExperiment, "min_minutes_between_updates", 15};

  // Whether ItemSuggest should be queried more than once per session. Multiple
  // queries are issued if either this param is true or the suggested files
  // experiment is enabled.
  static constexpr base::FeatureParam<bool> kMultipleQueriesPerSession{
      &kExperiment, "multiple_queries_per_session", false};

  // Returns the body for the itemsuggest request. Affected by |kExperiment|.
  std::string GetRequestBody();

  void OnTokenReceived(GoogleServiceAuthError error,
                       signin::AccessTokenInfo token_info);
  void OnSuggestionsReceived(const std::unique_ptr<std::string> json_response);
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);
  std::unique_ptr<network::SimpleURLLoader> MakeRequestLoader(
      const std::string& token);

  absl::optional<Results> results_;

  // Records the time of the last call to UpdateResults(), used to limit the
  // number of queries to the ItemSuggest backend.
  base::Time time_of_last_update_;

  // Start time for latency metrics.
  base::TimeTicks update_start_time_;

  // Whether the cache has made at least one request to ItemSuggest this
  // session. Used to prevent further updates in some cases.
  bool made_request_;

  const bool enabled_;
  const GURL server_url_;
  const base::TimeDelta min_time_between_updates_;
  // Whether we should query item suggest more than once per session.
  const bool multiple_queries_per_session_;

  Profile* profile_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ItemSuggestCache> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ITEM_SUGGEST_CACHE_H_
