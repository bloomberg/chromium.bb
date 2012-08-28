// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_FAKE_INVALIDATOR_H_
#define SYNC_NOTIFIER_FAKE_INVALIDATOR_H_

#include <string>

#include "base/compiler_specific.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/invalidator.h"
#include "sync/notifier/invalidator_registrar.h"

namespace syncer {

class FakeInvalidator : public Invalidator {
 public:
  FakeInvalidator();
  virtual ~FakeInvalidator();

  bool IsHandlerRegistered(InvalidationHandler* handler) const;
  ObjectIdSet GetRegisteredIds(InvalidationHandler* handler) const;
  const std::string& GetUniqueId() const;
  const std::string& GetStateDeprecated() const;
  const std::string& GetCredentialsEmail() const;
  const std::string& GetCredentialsToken() const;
  ModelTypeSet GetLastChangedTypes() const;

  virtual void RegisterHandler(InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredIds(InvalidationHandler* handler,
                                   const ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterHandler(InvalidationHandler* handler) OVERRIDE;
  virtual void SetUniqueId(const std::string& unique_id) OVERRIDE;
  virtual void SetStateDeprecated(const std::string& state) OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void SendNotification(ModelTypeSet changed_types) OVERRIDE;

 private:
  InvalidatorRegistrar registrar_;
  std::string unique_id_;
  std::string state_;
  std::string email_;
  std::string token_;
  ModelTypeSet last_changed_types_;
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_FAKE_INVALIDATOR_H_
