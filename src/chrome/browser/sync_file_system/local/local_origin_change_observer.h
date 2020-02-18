// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_ORIGIN_CHANGE_OBSERVER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_ORIGIN_CHANGE_OBSERVER_H_

#include <set>

#include "base/macros.h"
#include "url/gurl.h"

namespace sync_file_system {

class LocalOriginChangeObserver {
 public:
  LocalOriginChangeObserver() {}
  ~LocalOriginChangeObserver() {}

  virtual void OnChangesAvailableInOrigins(const std::set<GURL>& origins) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalOriginChangeObserver);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_ORIGIN_CHANGE_OBSERVER_H_
