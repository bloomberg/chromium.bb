// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_handler.h"

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "components/gwp_asan/crash_handler/crash_analyzer.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"
#include "third_party/crashpad/crashpad/snapshot/process_snapshot.h"

namespace gwp_asan {
namespace internal {
namespace {

using GwpAsanCrashAnalysisResult = CrashAnalyzer::GwpAsanCrashAnalysisResult;

// Return a serialized protobuf using a wrapper interface that
// crashpad::UserStreamDataSource expects us to return.
class BufferExtensionStreamDataSource final
    : public crashpad::MinidumpUserExtensionStreamDataSource {
 public:
  BufferExtensionStreamDataSource(uint32_t stream_type, Crash& crash);

  size_t StreamDataSize() override;
  bool ReadStreamData(Delegate* delegate) override;

 private:
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(BufferExtensionStreamDataSource);
};

BufferExtensionStreamDataSource::BufferExtensionStreamDataSource(
    uint32_t stream_type,
    Crash& crash)
    : crashpad::MinidumpUserExtensionStreamDataSource(stream_type) {
  bool result = crash.SerializeToString(&data_);
  DCHECK(result);
  ALLOW_UNUSED_LOCAL(result);
}

size_t BufferExtensionStreamDataSource::StreamDataSize() {
  DCHECK(!data_.empty());
  return data_.size();
}

bool BufferExtensionStreamDataSource::ReadStreamData(Delegate* delegate) {
  DCHECK(!data_.empty());
  return delegate->ExtensionStreamDataSourceRead(data_.data(), data_.size());
}

const char* ErrorToString(Crash_ErrorType type) {
  switch (type) {
    case Crash::USE_AFTER_FREE:
      return "heap-use-after-free";
    case Crash::BUFFER_UNDERFLOW:
      return "heap-buffer-underflow";
    case Crash::BUFFER_OVERFLOW:
      return "heap-buffer-overflow";
    case Crash::DOUBLE_FREE:
      return "double-free";
    case Crash::UNKNOWN:
      return "unknown";
    default:
      return "unexpected error type";
  }
}

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
HandleException(const crashpad::ProcessSnapshot& snapshot) {
  gwp_asan::Crash proto;
  auto result = CrashAnalyzer::GetExceptionInfo(snapshot, &proto);
  UMA_HISTOGRAM_ENUMERATION("GwpAsan.CrashAnalysisResult", result);

  if (result != GwpAsanCrashAnalysisResult::kGwpAsanCrash)
    return nullptr;

  LOG(ERROR) << "Detected GWP-ASan crash for allocation at 0x" << std::hex
             << proto.allocation_address() << std::dec << " of type "
             << ErrorToString(proto.error_type());

  return std::make_unique<BufferExtensionStreamDataSource>(
      kGwpAsanMinidumpStreamType, proto);
}

}  // namespace
}  // namespace internal

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
UserStreamDataSource::ProduceStreamData(crashpad::ProcessSnapshot* snapshot) {
  if (!snapshot) {
    DLOG(ERROR) << "Null process snapshot is unexpected.";
    return nullptr;
  }

  return internal::HandleException(*snapshot);
}

}  // namespace gwp_asan
