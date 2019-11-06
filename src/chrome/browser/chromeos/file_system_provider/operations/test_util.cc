// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/test_util.h"

#include <utility>

#include "extensions/browser/event_router.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace util {

LoggingDispatchEventImpl::LoggingDispatchEventImpl(bool dispatch_reply)
    : dispatch_reply_(dispatch_reply) {
}

LoggingDispatchEventImpl::~LoggingDispatchEventImpl() {
}

bool LoggingDispatchEventImpl::OnDispatchEventImpl(
    std::unique_ptr<extensions::Event> event) {
  events_.push_back(std::move(event));
  return dispatch_reply_;
}

void LogStatusCallback(StatusCallbackLog* log, base::File::Error result) {
  log->push_back(result);
}

}  // namespace util
}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
