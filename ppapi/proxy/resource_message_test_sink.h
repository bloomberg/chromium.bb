// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_RESOURCE_MESSAGE_TEST_SINK_H_
#define PPAPI_PROXY_RESOURCE_MESSAGE_TEST_SINK_H_

#include "ipc/ipc_test_sink.h"

namespace ppapi {
namespace proxy {

class ResourceMessageCallParams;
class ResourceMessageReplyParams;

// Extends IPC::TestSink to add extra capabilities for searching for and
// decoding resource messages.
class ResourceMessageTestSink : public IPC::TestSink {
 public:
  ResourceMessageTestSink();
  virtual ~ResourceMessageTestSink();

  // Searches the queue for the first resource call message with a nested
  // message matching the given ID. On success, returns true and populates the
  // givem params and nested message.
  bool GetFirstResourceCallMatching(
      uint32 id,
      ResourceMessageCallParams* params,
      IPC::Message* nested_msg) const;

  // Like GetFirstResourceCallMatching except for replies.
  bool GetFirstResourceReplyMatching(
      uint32 id,
      ResourceMessageReplyParams* params,
      IPC::Message* nested_msg);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_RESOURCE_MESSAGE_TEST_SINK_H_
