// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_BLACKLIST_H_
#define CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_BLACKLIST_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist.h"

namespace base {
class Clock;
}

namespace blacklist {
class OptOutBlacklistDelegate;
class OptOutStore;
}  // namespace blacklist

// Page load capping only supports one type for the blacklist.
enum PageCappingBlacklistType {
  kPageCappingOnlyType = 0,
};

// A class that managers opt out blacklist parameters for the capping heavy
// pages feature.
class PageLoadCappingBlacklist : public blacklist::OptOutBlacklist {
 public:
  PageLoadCappingBlacklist(
      std::unique_ptr<blacklist::OptOutStore> opt_out_store,
      base::Clock* clock,
      blacklist::OptOutBlacklistDelegate* blacklist_delegate);
  ~PageLoadCappingBlacklist() override;

 protected:
  // OptOutBlacklist (virtual for testing):
  bool ShouldUseSessionPolicy(base::TimeDelta* duration,
                              size_t* history,
                              int* threshold) const override;
  bool ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                 size_t* history,
                                 int* threshold) const override;
  bool ShouldUseHostPolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold,
                           size_t* max_hosts) const override;
  bool ShouldUseTypePolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold) const override;
  blacklist::BlacklistData::AllowedTypesAndVersions GetAllowedTypes()
      const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageLoadCappingBlacklist);
};

#endif  // CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_BLACKLIST_H_
