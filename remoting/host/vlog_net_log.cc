// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/vlog_net_log.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"

namespace remoting {

VlogNetLog::VlogNetLog() : id_(0) {
}

VlogNetLog::~VlogNetLog() {
}

void VlogNetLog::OnAddEntry(const NetLog::Entry& entry) {
  if (VLOG_IS_ON(4)) {
    scoped_ptr<Value> value(entry.ToValue());
    std::string json;
    base::JSONWriter::Write(value.get(), &json);
    VLOG(4) << json;
  }
}

uint32 VlogNetLog::NextID() {
  // TODO(mmenke):  Make this threadsafe and start with 1 instead of 0.
  return id_++;
}

net::NetLog::LogLevel VlogNetLog::GetLogLevel() const {
  return LOG_ALL_BUT_BYTES;
}

void VlogNetLog::AddThreadSafeObserver(ThreadSafeObserver* observer,
                                      net::NetLog::LogLevel log_level) {
  NOTIMPLEMENTED();
}

void VlogNetLog::SetObserverLogLevel(ThreadSafeObserver* observer,
                                    net::NetLog::LogLevel log_level) {
  NOTIMPLEMENTED();
}

void VlogNetLog::RemoveThreadSafeObserver(ThreadSafeObserver* observer) {
  NOTIMPLEMENTED();
}

}  // namespace remoting
