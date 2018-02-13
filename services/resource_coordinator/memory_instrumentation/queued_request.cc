// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/queued_request.h"

namespace memory_instrumentation {

QueuedRequest::Args::Args(MemoryDumpType dump_type,
                          MemoryDumpLevelOfDetail level_of_detail,
                          const std::vector<std::string>& allocator_dump_names,
                          bool add_to_trace,
                          base::ProcessId pid)
    : dump_type(dump_type),
      level_of_detail(level_of_detail),
      allocator_dump_names(allocator_dump_names),
      add_to_trace(add_to_trace),
      pid(pid) {}
QueuedRequest::Args::Args(const Args& args) = default;
QueuedRequest::Args::~Args() = default;

QueuedRequest::Response::Response() {}
QueuedRequest::Response::~Response() = default;

QueuedRequest::QueuedRequest(
    const Args& args,
    uint64_t dump_guid,
    const RequestGlobalMemoryDumpInternalCallback& callback)
    : args(args), dump_guid(dump_guid), callback(callback) {}
QueuedRequest::~QueuedRequest() = default;

base::trace_event::MemoryDumpRequestArgs QueuedRequest::GetRequestArgs() {
  base::trace_event::MemoryDumpRequestArgs request_args;
  request_args.dump_guid = dump_guid;
  request_args.dump_type = args.dump_type;
  request_args.level_of_detail = args.level_of_detail;
  return request_args;
}

QueuedRequest::PendingResponse::PendingResponse(
    const mojom::ClientProcess* client,
    Type type)
    : client(client), type(type) {}

bool QueuedRequest::PendingResponse::operator<(
    const PendingResponse& other) const {
  return std::tie(client, type) < std::tie(other.client, other.type);
}

}  // namespace memory_instrumentation