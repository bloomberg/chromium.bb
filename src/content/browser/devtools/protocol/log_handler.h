// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_LOG_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_LOG_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/log.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/leak_detector/leak_detector.mojom.h"

namespace content {

class DevToolsAgentHostImpl;

namespace protocol {

class LogHandler final : public DevToolsDomainHandler, public Log::Backend {
 public:
  LogHandler();
  ~LogHandler() override;

  static std::vector<LogHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  // DevToolsDomainHandler implementation.
  void Wire(UberDispatcher* dispatcher) override;

  // Log::Backend implementation.
  DispatchResponse Disable() override;
  DispatchResponse Enable() override;

  void EntryAdded(std::unique_ptr<Log::LogEntry> entry);

 private:
  std::unique_ptr<Log::Frontend> frontend_;
  bool enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(LogHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_LOG_HANDLER_H_
