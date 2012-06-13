// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_GDIG_FILE_NET_LOG_H_
#define NET_TOOLS_GDIG_FILE_NET_LOG_H_
#pragma once

#include <string>

#include "base/atomic_sequence_num.h"
#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "net/base/net_log.h"

namespace net {

// FileNetLog is a simple implementation of NetLog that prints out all
// the events received into the stream passed to the constructor.
class FileNetLog : public NetLog {
 public:
  explicit FileNetLog(FILE* destination, LogLevel level);
  virtual ~FileNetLog();

 private:
  // NetLog implementation:
  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE;
  virtual uint32 NextID() OVERRIDE;
  virtual LogLevel GetLogLevel() const OVERRIDE;
  virtual void AddThreadSafeObserver(ThreadSafeObserver* observer,
                                     LogLevel log_level) OVERRIDE;
  virtual void SetObserverLogLevel(ThreadSafeObserver* observer,
                                   LogLevel log_level) OVERRIDE;
  virtual void RemoveThreadSafeObserver(ThreadSafeObserver* observer) OVERRIDE;

  base::AtomicSequenceNumber sequence_number_;
  const NetLog::LogLevel log_level_;

  FILE* const destination_;
  base::Lock lock_;

  base::Time first_event_time_;
};

}  // namespace net

#endif  // NET_TOOLS_GDIG_FILE_NET_LOG_H_
