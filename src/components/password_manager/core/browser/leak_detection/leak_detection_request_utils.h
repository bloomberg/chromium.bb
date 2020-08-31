// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_UTILS_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/strings/string_piece_forward.h"
#include "base/task_runner.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"

namespace password_manager {

struct SingleLookupResponse;

// Contains the payload for analysing one credential against the leaks.
struct LookupSingleLeakPayload {
  std::string username_hash_prefix;
  std::string encrypted_payload;
};

// Stores all the data needed for one credential lookup.
struct LookupSingleLeakData {
  LookupSingleLeakData() = default;
  LookupSingleLeakData(LookupSingleLeakData&& other) = default;
  LookupSingleLeakData& operator=(LookupSingleLeakData&& other) = default;
  ~LookupSingleLeakData() = default;

  LookupSingleLeakData(const LookupSingleLeakData&) = delete;
  LookupSingleLeakData& operator=(const LookupSingleLeakData&) = delete;

  LookupSingleLeakPayload payload;

  std::string encryption_key;
};

using SingleLeakRequestDataCallback =
    base::OnceCallback<void(LookupSingleLeakData)>;

// Describes possible results of analyzing a leak response from the server.
// Needs to stay in sync with the PasswordAnalyzeLeakResponseResult enum in
// enums.xml.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AnalyzeResponseResult {
  kDecryptionError = 0,
  kNotLeaked = 1,
  kLeaked = 2,
  kMaxValue = kLeaked,
};

using SingleLeakResponseAnalysisCallback =
    base::OnceCallback<void(AnalyzeResponseResult)>;

// Asynchronously creates a data payload for single credential check.
// Callback is invoked on the calling thread with the protobuf and the
// encryption key used.
void PrepareSingleLeakRequestData(const std::string& username,
                                  const std::string& password,
                                  SingleLeakRequestDataCallback callback);

// Asynchronously creates a data payload for a credential check with the given
// encryption key. The task is posted to |task_runner|.
// Callback is invoked on the calling thread with the protobuf.
void PrepareSingleLeakRequestData(
    base::TaskRunner* task_runner,
    const std::string& encryption_key,
    const std::string& username,
    const std::string& password,
    base::OnceCallback<void(LookupSingleLeakPayload)> callback);

// Analyses the |response| asynchronously and checks if the credential was
// leaked. |callback| is invoked on the calling thread.
void AnalyzeResponse(std::unique_ptr<SingleLookupResponse> response,
                     const std::string& encryption_key,
                     SingleLeakResponseAnalysisCallback callback);

// Requests an access token for the API. |callback| is to be called with the
// result. The caller should keep the returned fetcher alive.
std::unique_ptr<signin::AccessTokenFetcher> RequestAccessToken(
    signin::IdentityManager* identity_manager,
    signin::AccessTokenFetcher::TokenCallback callback) WARN_UNUSED_RESULT;

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_UTILS_H_
