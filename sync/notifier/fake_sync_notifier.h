// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_FAKE_SYNC_NOTIFIER_H_
#define SYNC_NOTIFIER_FAKE_SYNC_NOTIFIER_H_

#include <string>

#include "base/compiler_specific.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/sync_notifier.h"
#include "sync/notifier/sync_notifier_registrar.h"

namespace syncer {

class FakeSyncNotifier : public SyncNotifier {
 public:
  FakeSyncNotifier();
  virtual ~FakeSyncNotifier();

  bool IsHandlerRegistered(SyncNotifierObserver* handler) const;
  ObjectIdSet GetRegisteredIds(SyncNotifierObserver* handler) const;
  const std::string& GetUniqueId() const;
  const std::string& GetStateDeprecated() const;
  const std::string& GetCredentialsEmail() const;
  const std::string& GetCredentialsToken() const;
  ModelTypeSet GetLastChangedTypes() const;

  virtual void RegisterHandler(SyncNotifierObserver* handler) OVERRIDE;
  virtual void UpdateRegisteredIds(SyncNotifierObserver* handler,
                                   const ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterHandler(SyncNotifierObserver* handler) OVERRIDE;
  virtual void SetUniqueId(const std::string& unique_id) OVERRIDE;
  virtual void SetStateDeprecated(const std::string& state) OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void SendNotification(ModelTypeSet changed_types) OVERRIDE;

 private:
  SyncNotifierRegistrar registrar_;
  std::string unique_id_;
  std::string state_;
  std::string email_;
  std::string token_;
  ModelTypeSet last_changed_types_;
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_FAKE_SYNC_NOTIFIER_H_
