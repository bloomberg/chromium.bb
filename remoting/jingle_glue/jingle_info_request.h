// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JINGLE_INFO_REQUEST_H_
#define REMOTING_JINGLE_GLUE_JINGLE_INFO_REQUEST_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"

class Task;

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace talk_base {
class SocketAddress;
}  // namespace talk_base

namespace remoting {

class IqRequest;

// JingleInfoRequest handles requesting STUN/Relay infromation from
// the Google Talk network. The query is made when Send() is
// called. The callback given to Send() is called when response to the
// request is received.
//
// This class is not threadsafe and should be used on the same thread it is
// created on.
//
// TODO(ajwong): Add support for a timeout.
class JingleInfoRequest : public sigslot::has_slots<> {
 public:
  // Callback to receive the Jingle configuration settings.  The argumetns are
  // passed by pointer so the receive may call swap on them.  The receiver does
  // NOT own the arguments, which are guaranteed only to be alive for the
  // duration of the callback.
  typedef base::Callback<void (
      const std::string&, const std::vector<std::string>&,
      const std::vector<talk_base::SocketAddress>&)> OnJingleInfoCallback;

  explicit JingleInfoRequest(IqRequest* request);
  virtual ~JingleInfoRequest();

  void Send(const OnJingleInfoCallback& callback);

 private:
  struct PendingDnsRequest;

  void OnResponse(const buzz::XmlElement* stanza);

  scoped_ptr<IqRequest> request_;
  OnJingleInfoCallback on_jingle_info_cb_;

  DISALLOW_COPY_AND_ASSIGN(JingleInfoRequest);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JINGLE_INFO_REQUEST_H_
