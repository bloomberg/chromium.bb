#ifndef NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_RSP_SESSION_LOG_H_
#define NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_RSP_SESSION_LOG_H_

#pragma once
#include <string>
#include "debug_blob.h"

namespace rsp {
class SessionLog {
 public:
  SessionLog();
  ~SessionLog();

  bool OpenToWrite(const std::string& file_name);
  bool OpenToRead(const std::string& file_name);
  void Close();
  void Add(char record_type, const debug::Blob& record);
  bool GetNextRecord(char* record_type, debug::Blob* record);

 protected:
  FILE* file_;
};
}  // namespace rsp

#endif  // NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_RSP_SESSION_LOG_H_