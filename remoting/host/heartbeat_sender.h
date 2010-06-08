// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HEARTBEAT_SENDER_H_
#define REMOTING_HOST_HEARTBEAT_SENDER_H_

#include <string>

#include "base/scoped_ptr.h"
#include "base/ref_counted.h"
#include "remoting/jingle_glue/iq_request.h"

namespace remoting {

class IqRequest;
class JingleClient;

// HeartbeatSender periodically sends hertbeats to the chromoting bot.
// TODO(sergeyu): Write unittest for this class.
class HeartbeatSender : public base::RefCountedThreadSafe<HeartbeatSender> {
 public:
  HeartbeatSender();

  // Starts heart-beating for |jingle_client|.
  void Start(JingleClient* jingle_client, const std::string& host_id);

 private:
  void DoStart();
  void DoSendStanza();

  void ProcessResponse(const buzz::XmlElement* response);

  bool started_;
  JingleClient* jingle_client_;
  std::string host_id_;
  scoped_ptr<IqRequest> request_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HEARTBEAT_SENDER_H_
