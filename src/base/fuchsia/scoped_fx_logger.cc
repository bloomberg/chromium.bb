// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_fx_logger.h"

#include <lib/fdio/directory.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/process/process.h"
#include "base/strings/string_piece.h"
#include "base/threading/platform_thread.h"

namespace base {

ScopedFxLogger::ScopedFxLogger() = default;
ScopedFxLogger::~ScopedFxLogger() = default;

ScopedFxLogger::ScopedFxLogger(ScopedFxLogger&& other) = default;
ScopedFxLogger& ScopedFxLogger::operator=(ScopedFxLogger&& other) = default;

// static
ScopedFxLogger ScopedFxLogger::CreateForProcess(
    std::vector<base::StringPiece> tags) {
  // CHECK()ing or LOG()ing inside this function is safe, since it is only
  // called to initialize logging, not during individual logging operations.

  fuchsia::logger::LogSinkHandle log_sink;
  zx_status_t status =
      fdio_service_connect("/svc/fuchsia.logger.LogSink",
                           log_sink.NewRequest().TakeChannel().release());
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "connect(LogSink) failed";
    return {};
  }

  // Rather than relying on automatic LogSink attribution via COMPONENT_NAME,
  // prepend a tag based on the calling process' name.  COMPONENT_NAME may be
  // mis-attributed, in some Component configurations, to a parent or caller
  // component, from which the process' LogSink service is routed.
  std::string program_name = base::CommandLine::ForCurrentProcess()
                                 ->GetProgram()
                                 .BaseName()
                                 .AsUTF8Unsafe();
  tags.insert(tags.begin(), program_name);

  return CreateFromLogSink(std::move(log_sink), std::move(tags));
}

// static
ScopedFxLogger ScopedFxLogger::CreateFromLogSink(
    fuchsia::logger::LogSinkHandle log_sink_handle,
    std::vector<base::StringPiece> tags) {
  // CHECK()ing or LOG()ing inside this function is safe, since it is only
  // called to initialize logging, not during individual logging operations.

  // Attempts to create a kernel socket object should never fail.
  zx::socket local, remote;
  zx_status_t status = zx::socket::create(ZX_SOCKET_DATAGRAM, &local, &remote);
  ZX_CHECK(status == ZX_OK, status) << "zx_socket_create() failed";

  // ConnectStructured() may fail if e.g. the LogSink has disconnected already.
  fuchsia::logger::LogSinkSyncPtr log_sink;
  log_sink.Bind(std::move(log_sink_handle));
  status = log_sink->ConnectStructured(std::move(remote));
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "ConnectStructured() failed";
    return {};
  }

  return ScopedFxLogger(std::move(tags), std::move(local));
}

void ScopedFxLogger::LogMessage(base::StringPiece file,
                                uint32_t line_number,
                                base::StringPiece msg,
                                FuchsiaLogSeverity severity) {
  if (!socket_.is_valid())
    return;

  // It is not safe to use e.g. CHECK() or LOG() macros here, since those
  // may result in reentrancy if this instance is used for routing process-
  // global logs to the system logger.

  fuchsia_syslog::LogBuffer buffer;
  buffer.BeginRecord(severity, cpp17::string_view(file.data(), file.size()),
                     line_number, cpp17::string_view(msg.data(), msg.size()),
                     false, socket_.borrow(), 0, base::Process::Current().Pid(),
                     base::PlatformThread::CurrentId());
  for (const auto& tag : tags_) {
    buffer.WriteKeyValue("tag", tag);
  }
  buffer.FlushRecord();
}

ScopedFxLogger::ScopedFxLogger(std::vector<base::StringPiece> tags,
                               zx::socket socket)
    : tags_(tags.begin(), tags.end()), socket_(std::move(socket)) {}

}  // namespace base
