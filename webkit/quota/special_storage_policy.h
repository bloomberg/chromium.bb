// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_SPECIAL_STORAGE_POLICY_H_
#define WEBKIT_QUOTA_SPECIAL_STORAGE_POLICY_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "webkit/storage/webkit_storage_export.h"

class GURL;

namespace quota {

// Special rights are granted to 'extensions' and 'applications'. The
// storage subsystems query this interface to determine which origins
// have these rights. Chrome provides an impl that is cognizant of what
// is currently installed in the extensions system.
// The IsSomething() methods must be thread-safe, however Observers should
// only be notified, added, and removed on the IO thead.
class WEBKIT_STORAGE_EXPORT SpecialStoragePolicy
    : public base::RefCountedThreadSafe<SpecialStoragePolicy> {
 public:
  class Observer {
   public:
    virtual void OnSpecialStoragePolicyChanged() = 0;
   protected:
    virtual ~Observer();
  };

  SpecialStoragePolicy();

  // Protected storage is not subject to removal by the browsing data remover.
  virtual bool IsStorageProtected(const GURL& origin) = 0;

  // Unlimited storage is not subject to 'quotas'.
  virtual bool IsStorageUnlimited(const GURL& origin) = 0;

  // Installed apps have access to the size of the remaining disk capacity.
  virtual bool IsInstalledApp(const GURL& origin) = 0;

  // Checks if extension identified with |extension_id| is registered as
  // file handler.
  virtual bool IsFileHandler(const std::string& extension_id) = 0;

  // Some origins are only allowed to store session-only data which is deleted
  // when the session ends.
  virtual bool IsStorageSessionOnly(const GURL& origin) = 0;

  // Returns true if some origins are only allowed session-only storage.
  virtual bool HasSessionOnlyOrigins() = 0;

  // Adds/removes an observer, the policy does not take
  // ownership of the observer. Should only be called on the IO thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  friend class base::RefCountedThreadSafe<SpecialStoragePolicy>;
  virtual ~SpecialStoragePolicy();
  void NotifyObservers();

  ObserverList<Observer> observers_;
};

}  // namespace quota

#endif  // WEBKIT_QUOTA_SPECIAL_STORAGE_POLICY_H_
