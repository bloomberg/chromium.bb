// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_SPECIAL_STORAGE_POLICY_H_
#define STORAGE_BROWSER_QUOTA_SPECIAL_STORAGE_POLICY_H_

#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "services/network/session_cleanup_cookie_store.h"

class GURL;

namespace url {
class Origin;
}

namespace storage {

// Special rights are granted to 'extensions' and 'applications'. The
// storage subsystems query this interface to determine which origins
// have these rights. Chrome provides an impl that is cognizant of what
// is currently installed in the extensions system.
// The IsSomething() methods must be thread-safe, however Observers should
// only be notified, added, and removed on the IO thead.
class COMPONENT_EXPORT(STORAGE_BROWSER) SpecialStoragePolicy
    : public base::RefCountedThreadSafe<SpecialStoragePolicy> {
 public:
  using StoragePolicy = int;
  enum ChangeFlags {
    STORAGE_PROTECTED = 1 << 0,
    STORAGE_UNLIMITED = 1 << 1,
  };

  class COMPONENT_EXPORT(STORAGE_BROWSER) Observer {
   public:
    // Called when one or more features corresponding to |change_flags| have
    // been granted for |origin| storage.
    virtual void OnGranted(const url::Origin& origin, int change_flags) {}

    // Called when one or more features corresponding to |change_flags| have
    // been revoked for |origin| storage.
    virtual void OnRevoked(const url::Origin& origin, int change_flags) {}

    // Called when all features corresponding to ChangeFlags have been revoked
    // for all origins.
    virtual void OnCleared() {}

    // Called any time the policy changes in any meaningful way, i.e., the
    // public Is/Has querying methods may return different values from before
    // this notification.
    virtual void OnPolicyChanged() {}

   protected:
    virtual ~Observer();
  };

  SpecialStoragePolicy();

  // Protected storage is not subject to removal by the browsing data remover.
  virtual bool IsStorageProtected(const GURL& origin) = 0;

  // Unlimited storage is not subject to quota or storage pressure eviction.
  virtual bool IsStorageUnlimited(const GURL& origin) = 0;

  // Durable storage is not subject to storage pressure eviction.
  virtual bool IsStorageDurable(const GURL& origin) = 0;

  // Checks if the origin contains per-site isolated storage.
  virtual bool HasIsolatedStorage(const GURL& origin) = 0;

  // Some origins are only allowed to store session-only data which is deleted
  // when the session ends.
  virtual bool IsStorageSessionOnly(const GURL& origin) = 0;

  // Returns true if some origins are only allowed session-only storage.
  virtual bool HasSessionOnlyOrigins() = 0;

  // Returns a predicate that takes the domain of a cookie and a bool whether
  // the cookie is secure and returns true if the cookie should be deleted on
  // exit.
  // If |HasSessionOnlyOrigins()| is true a non-null callback is returned.
  // It uses domain matching as described in section 5.1.3 of RFC 6265 to
  // identify content setting rules that could have influenced the cookie
  // when it was created.
  virtual network::SessionCleanupCookieStore::DeleteCookiePredicate
  CreateDeleteCookieOnExitPredicate() = 0;

  // Adds/removes an observer, the policy does not take
  // ownership of the observer. Should only be called on the IO thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  friend class base::RefCountedThreadSafe<SpecialStoragePolicy>;
  virtual ~SpecialStoragePolicy();

  // Notify observes of specific policy changes. Note that all of these also
  // implicitly invoke |NotifyPolicyChanged()|.
  void NotifyGranted(const url::Origin& origin, int change_flags);
  void NotifyRevoked(const url::Origin& origin, int change_flags);
  void NotifyCleared();

  // Subclasses can call this for any policy changes which don't fit any of the
  // above notifications.
  void NotifyPolicyChanged();

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_SPECIAL_STORAGE_POLICY_H_
