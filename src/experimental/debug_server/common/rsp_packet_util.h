#ifndef NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_RSP_PACKET_UTILS_H_
#define NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_RSP_PACKET_UTILS_H_

#pragma once
#include <string>
#include <vector>
#include <deque>
#include <map>
#include "debug_blob.h"

namespace rsp {
class PacketUtils {
 public:
  static void AddEnvelope(const debug::Blob& blob_in, debug::Blob* blob_out);
  static bool RemoveEnvelope(const debug::Blob& blob_in, debug::Blob* blob_out);
};
}  //namespace rsp

#endif  // NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_COMMON_RSP_PACKET_UTILS_H_