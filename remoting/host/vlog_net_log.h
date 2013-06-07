// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_VLOG_NET_LOG_H_
#define REMOTING_HOST_VLOG_NET_LOG_H_

#include "base/memory/scoped_handle.h"
#include "net/base/net_log.h"

namespace remoting {

// Redirectes all networking events (i.e. events logged through net::NetLog) to
// VLOG(4). Note that an explicit reference to a net::NetLog object has to be
// passed to networking classes to receive the events. There is no global
// network events logger exists.
class VlogNetLog : public net::NetLog {
 public:
  VlogNetLog();
  virtual ~VlogNetLog();

  // NetLog overrides:
  virtual void OnAddEntry(const NetLog::Entry& entry) OVERRIDE;
  virtual uint32 NextID() OVERRIDE;
  virtual LogLevel GetLogLevel() const OVERRIDE;
  virtual void AddThreadSafeObserver(ThreadSafeObserver* observer,
                                     LogLevel log_level) OVERRIDE;
  virtual void SetObserverLogLevel(ThreadSafeObserver* observer,
                                   LogLevel log_level) OVERRIDE;
  virtual void RemoveThreadSafeObserver(ThreadSafeObserver* observer) OVERRIDE;

 private:
  uint32 id_;

  DISALLOW_COPY_AND_ASSIGN(VlogNetLog);
};

}  // namespace remoting

#endif  // REMOTING_HOST_VLOG_NET_LOG_H_
