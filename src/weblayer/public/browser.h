// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_BROWSER_H_
#define WEBLAYER_PUBLIC_BROWSER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

namespace weblayer {

class BrowserObserver;
class Profile;
class Tab;

// Represents an ordered list of Tabs, with one active. Browser does not own
// the set of Tabs.
class Browser {
 public:
  struct PersistenceInfo {
    PersistenceInfo();
    PersistenceInfo(const PersistenceInfo& other);
    ~PersistenceInfo();

    // Uniquely identifies this browser for session restore, empty is not a
    // valid id.
    std::string id;

    // Last key used to encrypt incognito profile.
    std::vector<uint8_t> last_crypto_key;

    // If non-empty used to restore the state of the browser. This is only used
    // if |id| is empty.
    std::vector<uint8_t> minimal_state;
  };

  // Creates a new Browser. |persistence_info|, if non-null, is used for saving
  // and restoring the state of the browser.
  static std::unique_ptr<Browser> Create(
      Profile* profile,
      const PersistenceInfo* persistence_info);

  virtual ~Browser() {}

  virtual Tab* AddTab(std::unique_ptr<Tab> tab) = 0;
  virtual std::unique_ptr<Tab> RemoveTab(Tab* tab) = 0;
  virtual void SetActiveTab(Tab* tab) = 0;
  virtual Tab* GetActiveTab() = 0;
  virtual std::vector<Tab*> GetTabs() = 0;

  // Called early on in shutdown, before any tabs have been removed.
  virtual void PrepareForShutdown() = 0;

  // Returns the id supplied to Create() that is used for persistence.
  virtual std::string GetPersistenceId() = 0;

  // Returns the tabs and navigations in a format suitable for serialization.
  // This state can be later restored via |PersistenceInfo::minimal_state|.
  // This is not the full state, only a minimal snapshot intended for
  // lightweight restore when full persistence is not desirable.
  virtual std::vector<uint8_t> GetMinimalPersistenceState() = 0;

  virtual void AddObserver(BrowserObserver* observer) = 0;
  virtual void RemoveObserver(BrowserObserver* observer) = 0;

  virtual void VisibleSecurityStateOfActiveTabChanged() = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_BROWSER_H_
