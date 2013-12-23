// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/vlog_net_log.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"

namespace remoting {

class VlogNetLog::Observer : public net::NetLog::ThreadSafeObserver {
 public:
  Observer();
  virtual ~Observer();

  // NetLog::ThreadSafeObserver overrides:
  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(Observer);
};

VlogNetLog::Observer::Observer() {
}

VlogNetLog::Observer::~Observer() {
}

void VlogNetLog::Observer::OnAddEntry(const net::NetLog::Entry& entry) {
  if (VLOG_IS_ON(4)) {
    scoped_ptr<base::Value> value(entry.ToValue());
    std::string json;
    base::JSONWriter::Write(value.get(), &json);
    VLOG(4) << json;
  }
}

VlogNetLog::VlogNetLog()
    : observer_(new Observer()) {
  AddThreadSafeObserver(observer_.get(), LOG_ALL_BUT_BYTES);
}

VlogNetLog::~VlogNetLog() {
  RemoveThreadSafeObserver(observer_.get());
}

}  // namespace remoting
