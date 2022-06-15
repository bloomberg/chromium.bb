// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_DMSERVER_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_DMSERVER_CLIENT_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

// Indicates all possible PSM protocol results after it has executed
// successfully or terminated because of an error or timeout. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class PsmResult {
  kSuccessfulDetermination = 0,
  kCreateRlweClientLibraryError = 1,
  kCreateOprfRequestLibraryError = 2,
  kCreateQueryRequestLibraryError = 3,
  kProcessingQueryResponseLibraryError = 4,
  kEmptyOprfResponseError = 5,
  kEmptyQueryResponseError = 6,
  kConnectionError = 7,
  kServerError = 8,
  kTimeout = 9,
  kMaxValue = kTimeout,
};

// Interface for the PSM RLWE Client which uses DMServer, allowing to replace
// the PSM RLWE DMServer client with a fake for tests.
class PsmRlweDmserverClient {
 public:
  struct ResultHolder final {
    explicit ResultHolder(
        PsmResult psm_result,
        absl::optional<bool> membership_result = absl::nullopt,
        absl::optional<base::Time> membership_determination_time =
            absl::nullopt)
        : psm_result(psm_result),
          membership_result(membership_result),
          membership_determination_time(membership_determination_time) {}

    // Indicate whether an error occurred while executing the PSM protocol.
    bool IsError() const {
      return psm_result != PsmResult::kSuccessfulDetermination;
    }

    PsmResult psm_result;

    // These fields have values only if `psm_result` value is
    // `kSuccessfulDetermination`.

    absl::optional<bool> membership_result;
    absl::optional<base::Time> membership_determination_time;
  };

  // Callback will be triggered after completing the protocol, in case of a
  // successful determination or stopping due to an error.
  using CompletionCallback =
      base::OnceCallback<void(ResultHolder result_holder)>;

  virtual ~PsmRlweDmserverClient() = default;

  // Determines the membership. Then, will call `callback` on
  // successful result or error. Also, the `callback` has to be non-null.
  // Note: This method should be called only when there is no PSM request in
  // progress (i.e. `IsCheckMembershipInProgress` is false).
  virtual void CheckMembership(CompletionCallback callback) = 0;

  // Returns true if the PSM protocol is still running,
  // otherwise false.
  virtual bool IsCheckMembershipInProgress() const = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_DMSERVER_CLIENT_H_
