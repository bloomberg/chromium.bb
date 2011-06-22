// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JINGLE_INFO_REQUEST_H_
#define REMOTING_JINGLE_GLUE_JINGLE_INFO_REQUEST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "base/memory/scoped_ptr.h"

class Task;

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace talk_base {
class SocketAddress;
}  // namespace talk_base

namespace remoting {

class IqRequest;

// JingleInfoRequest handles making an IQ request to the Google talk network for
// discovering stun/relay information for use in establishing a Jingle
// connection.
//
// Clients should instantiate this class, and set a callback to receive the
// configuration information.  The query will be made when Run() is called.  The
// query is finisehd when the |done| task given to Run() is invokved.
//
// This class is not threadsafe and should be used on the same thread it is
// created on.
//
// TODO(ajwong): Add support for a timeout.
class JingleInfoRequest {
 public:
  // Callback to receive the Jingle configuration settings.  The argumetns are
  // passed by pointer so the receive may call swap on them.  The receiver does
  // NOT own the arguments, which are guaranteed only to be alive for the
  // duration of the callback.
  typedef Callback3<const std::string&, const std::vector<std::string>&,
                    const std::vector<talk_base::SocketAddress>&>::Type
      OnJingleInfoCallback;

  explicit JingleInfoRequest(IqRequest* request);
  ~JingleInfoRequest();

  void Run(Task* done);
  void SetCallback(OnJingleInfoCallback* callback);
  void DetachCallback();

 private:
  void OnResponse(const buzz::XmlElement* stanza);

  scoped_ptr<IqRequest> request_;
  scoped_ptr<OnJingleInfoCallback> on_jingle_info_cb_;
  scoped_ptr<Task> done_cb_;

  DISALLOW_COPY_AND_ASSIGN(JingleInfoRequest);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JINGLE_INFO_REQUEST_H_
