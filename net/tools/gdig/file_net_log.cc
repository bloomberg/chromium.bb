// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/tools/gdig/file_net_log.h"

namespace net {

FileNetLog::FileNetLog(FILE* destination, LogLevel level)
    : log_level_(level),
      destination_(destination) {
  DCHECK(destination != NULL);
  // Without calling GetNext() once here, the first GetNext will return 0
  // that is not a valid id.
  sequence_number_.GetNext();
}

FileNetLog::~FileNetLog() {
}

void FileNetLog::OnAddEntry(const net::NetLog::Entry& entry) {
  // Only BoundNetLogs without a NetLog should have an invalid source.
  DCHECK(entry.source().IsValid());

  const char* source = SourceTypeToString(entry.source().type);
  const char* type = EventTypeToString(entry.type());

  scoped_ptr<Value> param_value(entry.ParametersToValue());
  std::string params;
  if (param_value.get() != NULL) {
    JSONStringValueSerializer serializer(&params);
    bool ret = serializer.Serialize(*param_value);
    DCHECK(ret);
  }
  base::Time now = base::Time::NowFromSystemTime();
  base::AutoLock lock(lock_);
  if (first_event_time_.is_null()) {
    first_event_time_ = now;
  }
  base::TimeDelta elapsed_time = now - first_event_time_;
  fprintf(destination_ , "%u\t%u\t%s\t%s\t%s\n",
          static_cast<unsigned>(elapsed_time.InMilliseconds()),
          entry.source().id, source, type, params.c_str());
}

uint32 FileNetLog::NextID() {
  return sequence_number_.GetNext();
}

NetLog::LogLevel FileNetLog::GetLogLevel() const {
  return log_level_;
}

void FileNetLog::AddThreadSafeObserver(
    NetLog::ThreadSafeObserver* observer,
    NetLog::LogLevel log_level) {
  NOTIMPLEMENTED() << "Not currently used by gdig.";
}

void FileNetLog::SetObserverLogLevel(ThreadSafeObserver* observer,
                                          LogLevel log_level) {
  NOTIMPLEMENTED() << "Not currently used by gdig.";
}

void FileNetLog::RemoveThreadSafeObserver(
    NetLog::ThreadSafeObserver* observer) {
  NOTIMPLEMENTED() << "Not currently used by gdig.";
}

}  // namespace net
