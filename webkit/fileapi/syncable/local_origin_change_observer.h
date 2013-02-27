// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_LOCAL_ORIGIN_CHANGE_OBSERVER_H_
#define WEBKIT_FILEAPI_SYNCABLE_LOCAL_ORIGIN_CHANGE_OBSERVER_H_

#include <set>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

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

#endif  // WEBKIT_FILEAPI_SYNCABLE_LOCAL_ORIGIN_CHANGE_OBSERVER_H_
