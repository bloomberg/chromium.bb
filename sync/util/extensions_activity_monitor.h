// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_EXTENSIONS_ACTIVITY_MONITOR_H_
#define SYNC_UTIL_EXTENSIONS_ACTIVITY_MONITOR_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "sync/base/sync_export.h"

namespace syncer {

// An interface to monitor usage of extensions APIs to send to sync
// servers, with the ability to purge data once sync servers have
// acknowledged it (successful commit response).
//
// All abstract methods are called from the sync thread.
class SYNC_EXPORT ExtensionsActivityMonitor {
 public:
  // A data record of activity performed by extension |extension_id|.
  struct SYNC_EXPORT Record {
    Record();
    ~Record();

    // The human-readable ID identifying the extension responsible
    // for the activity reported in this Record.
    std::string extension_id;

    // How many times the extension successfully invoked a write
    // operation through the bookmarks API since the last CommitMessage.
    uint32 bookmark_write_count;
  };

  typedef std::map<std::string, Record> Records;

  // Fill |buffer| with all current records and then clear the
  // internal records.
  virtual void GetAndClearRecords(Records* buffer) = 0;

  // Merge |records| with the current set of records, adding the
  // bookmark write counts for common Records.
  virtual void PutRecords(const Records& records) = 0;

 protected:
  virtual ~ExtensionsActivityMonitor();
};

}  // namespace syncer

#endif  // SYNC_UTIL_EXTENSIONS_ACTIVITY_MONITOR_H_
