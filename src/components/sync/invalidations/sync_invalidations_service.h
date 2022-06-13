// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_
#define COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/model_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {
class FCMRegistrationTokenObserver;
class InvalidationsListener;
class InterestedDataTypesHandler;

// Service which is used to register with FCM. It is used to obtain an FCM token
// which is used to send invalidations from the server. The service also
// provides incoming invalidations handling and an interface to subscribe to
// invalidations. To subscribe for invalidations a new InvalidationsListener
// should be added.
class SyncInvalidationsService : public KeyedService {
 public:
  // Data types which are newly marked as interesting will be passed to the
  // callback.
  using InterestedDataTypesAppliedCallback =
      base::RepeatingCallback<void(const ModelTypeSet&)>;

  // Start or stop listening to invalidations.
  virtual void SetActive(bool active) = 0;

  // Add or remove a new listener which will be notified on each new incoming
  // invalidation. |listener| must not be nullptr. If there is no such
  // |listener| then RemoveListener will do nothing.
  virtual void AddListener(InvalidationsListener* listener) = 0;
  virtual void RemoveListener(InvalidationsListener* listener) = 0;

  // Add or remove an FCM token change observer. |observer| must not be nullptr.
  virtual void AddTokenObserver(FCMRegistrationTokenObserver* observer) = 0;
  virtual void RemoveTokenObserver(FCMRegistrationTokenObserver* observer) = 0;

  // Used to get an obtained FCM token. absl::nullopt is returned if the token
  // has been requested but hasn't been received yet. Returns an empty string if
  // the device is not listening to invalidations.
  virtual absl::optional<std::string> GetFCMRegistrationToken() const = 0;

  // Set the interested data types change handler. |handler| can be nullptr to
  // unregister any existing handler. There can be at most one handler.
  virtual void SetInterestedDataTypesHandler(
      InterestedDataTypesHandler* handler) = 0;

  // Get or set for which data types should the device receive invalidations.
  // GetInterestedDataTypes() will return base::nullptr until
  // SetInterestedDataTypes() has been called at least once.
  virtual absl::optional<ModelTypeSet> GetInterestedDataTypes() const = 0;
  virtual void SetInterestedDataTypes(const ModelTypeSet& data_types) = 0;
  virtual void SetCommittedAdditionalInterestedDataTypesCallback(
      InterestedDataTypesAppliedCallback callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_
