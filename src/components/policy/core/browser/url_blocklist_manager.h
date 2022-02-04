// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_URL_BLOCKLIST_MANAGER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_URL_BLOCKLIST_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/policy_export.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class ListValue;
class SequencedTaskRunner;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {

// Contains a set of filters to block and allow certain URLs, and matches GURLs
// against this set. The filters are currently kept in memory.
class POLICY_EXPORT URLBlocklist {
 public:
  // Indicates if the URL matches a pattern defined in blocklist, in allowlist
  // or doesn't match anything in either list as defined in URLBlocklist and
  // URLAllowlist policies.
  enum URLBlocklistState {
    URL_IN_ALLOWLIST,
    URL_IN_BLOCKLIST,
    URL_NEUTRAL_STATE,
  };

  URLBlocklist();
  URLBlocklist(const URLBlocklist&) = delete;
  URLBlocklist& operator=(const URLBlocklist&) = delete;
  virtual ~URLBlocklist();

  // URLs matching one of the |filters| will be blocked. The filter format is
  // documented at
  // http://www.chromium.org/administrators/url-blocklist-filter-format.
  void Block(const base::ListValue* filters);

  // URLs matching one of the |filters| will be allowed. If a URL is both
  // Blocked and Allowed, Allow takes precedence.
  void Allow(const base::ListValue* filters);

  // Returns true if the URL is blocked.
  bool IsURLBlocked(const GURL& url) const;

  URLBlocklistState GetURLBlocklistState(const GURL& url) const;

  // Returns the number of items in the list.
  size_t Size() const;

 private:
  // Returns true if |lhs| takes precedence over |rhs|.
  static bool FilterTakesPrecedence(
      const url_matcher::util::FilterComponents& lhs,
      const url_matcher::util::FilterComponents& rhs);

  url_matcher::URLMatcherConditionSet::ID id_ = 0;
  std::map<url_matcher::URLMatcherConditionSet::ID,
           url_matcher::util::FilterComponents>
      filters_;
  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
};

// Tracks the blocklist policies for a given profile, and updates it on changes.
class POLICY_EXPORT URLBlocklistManager {
 public:
  // Must be constructed on the UI thread.
  explicit URLBlocklistManager(PrefService* pref_service);
  URLBlocklistManager(const URLBlocklistManager&) = delete;
  URLBlocklistManager& operator=(const URLBlocklistManager&) = delete;
  virtual ~URLBlocklistManager();

  // Returns true if |url| is blocked by the current blocklist.
  bool IsURLBlocked(const GURL& url) const;

  URLBlocklist::URLBlocklistState GetURLBlocklistState(const GURL& url) const;

  // Replaces the current blocklist.
  // Virtual for testing.
  virtual void SetBlocklist(std::unique_ptr<URLBlocklist> blocklist);

  // Registers the preferences related to blocklisting in the given PrefService.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 protected:
  // Used to delay updating the blocklist while the preferences are
  // changing, and execute only one update per simultaneous prefs changes.
  void ScheduleUpdate();

  // Updates the blocklist using the current preference values.
  // Virtual for testing.
  virtual void Update();

 private:
  // Used to track the policies and update the blocklist on changes.
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<PrefService> pref_service_;  // Weak.

  // Used to post tasks to a background thread.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Used to schedule tasks on the main loop to avoid rebuilding the blocklist
  // multiple times during a message loop process. This can happen if two
  // preferences that change the blocklist are updated in one message loop
  // cycle.  In addition, we use this task runner to ensure that the
  // URLBlocklistManager is only access from the thread call the constructor for
  // data accesses.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // The current blocklist.
  std::unique_ptr<URLBlocklist> blocklist_;

  // Used to post update tasks to the UI thread.
  base::WeakPtrFactory<URLBlocklistManager> ui_weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_URL_BLOCKLIST_MANAGER_H_
