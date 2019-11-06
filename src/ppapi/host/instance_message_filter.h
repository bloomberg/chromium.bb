// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_INSTANCE_MESSAGE_FILTER_H_
#define PPAPI_HOST_INSTANCE_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "ppapi/host/ppapi_host_export.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace host {

class PpapiHost;

class PPAPI_HOST_EXPORT InstanceMessageFilter {
 public:
  explicit InstanceMessageFilter(PpapiHost* host);
  virtual ~InstanceMessageFilter();

  // Processes an instance message from the plugin process. Returns true if the
  // message was handled. On false, the PpapiHost will forward the message to
  // the next filter.
  virtual bool OnInstanceMessageReceived(const IPC::Message& msg) = 0;

  PpapiHost* host() { return host_; }

 private:
  PpapiHost* host_;

  DISALLOW_COPY_AND_ASSIGN(InstanceMessageFilter);
};

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_INSTANCE_MESSAGE_FILTER_H_
