// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/vlog_net_log.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "net/log/net_log.h"
#include "net/log/net_log_entry.h"

namespace remoting {

namespace {

class VlogNetLogObserver : public net::NetLog::ThreadSafeObserver {
 public:
  VlogNetLogObserver();
  ~VlogNetLogObserver() override;

  // NetLog::ThreadSafeObserver overrides:
  void OnAddEntry(const net::NetLogEntry& entry) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VlogNetLogObserver);
};

VlogNetLogObserver::VlogNetLogObserver() {
  // Only add the observer if verbosity is at least level 4. This is more
  // efficient than unconditionally adding the observer and checking the vlog
  // level in OnAddEntry.
  if (VLOG_IS_ON(4)) {
    net::NetLog::Get()->AddObserver(this,
                                    net::NetLogCaptureMode::kIncludeSensitive);
  }
}

VlogNetLogObserver::~VlogNetLogObserver() = default;

void VlogNetLogObserver::OnAddEntry(const net::NetLogEntry& entry) {
  base::Value value = entry.ToValue();
  std::string json;
  base::JSONWriter::Write(value, &json);
  VLOG(4) << json;
}

}  // namespace

void CreateVlogNetLogObserver() {
  static base::NoDestructor<VlogNetLogObserver> observer;
}

}  // namespace remoting
