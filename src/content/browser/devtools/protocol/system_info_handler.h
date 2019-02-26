// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_

#include <set>

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/system_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"

namespace content {
namespace protocol {

class SystemInfoHandler : public DevToolsDomainHandler,
                          public SystemInfo::Backend {
 public:

  SystemInfoHandler();
  ~SystemInfoHandler() override;

  void Wire(UberDispatcher* dispatcher) override;

  void GetInfo(std::unique_ptr<GetInfoCallback> callback) override;
  void GetProcessInfo(
      std::unique_ptr<GetProcessInfoCallback> callback) override;

 private:
  friend class SystemInfoHandlerGpuObserver;

  DISALLOW_COPY_AND_ASSIGN(SystemInfoHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_
