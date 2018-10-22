// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_SERVICE_H_
#define CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class FilePath;
}

class PageLoadCappingBlacklist;

// Keyed service that owns the page load capping blacklist.
class PageLoadCappingService : public KeyedService,
                               public blacklist::OptOutBlacklistDelegate {
 public:
  PageLoadCappingService();
  ~PageLoadCappingService() override;

  // Initializes the UI Service. |profile_path| is the path to user data on
  // disk.
  void Initialize(const base::FilePath& profile_path);

  PageLoadCappingBlacklist* page_load_capping_blacklist() const {
    return page_load_capping_blacklist_.get();
  }

 private:
  // blacklist::OptOutBlacklistDelegate:
  // TODO(ryansturm): Report page capping information to
  // interventions-internals. https://crbug.com/797988
  void OnNewBlacklistedHost(const std::string& host, base::Time time) override {
  }
  void OnUserBlacklistedStatusChange(bool blacklisted) override {}
  void OnBlacklistCleared(base::Time time) override {}

  // The blacklist used to control triggering of the page load capping feature.
  // Created during Initialize().
  std::unique_ptr<PageLoadCappingBlacklist> page_load_capping_blacklist_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadCappingService);
};

#endif  // CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_SERVICE_H_
