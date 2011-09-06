// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_IQ_REQUEST_H_
#define REMOTING_JINGLE_GLUE_IQ_REQUEST_H_

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {

// IqRequest class can be used to send an IQ stanza and then receive reply
// stanza for that request. It sends outgoing stanza when SendIq() is called,
// after that it forwards incoming reply stanza to the callback set with
// set_callback(). If each call to SendIq() will yield one invocation of the
// callback with the response.
class IqRequest {
 public:
  typedef base::Callback<void(const buzz::XmlElement*)> ReplyCallback;

  static buzz::XmlElement* MakeIqStanza(const std::string& type,
                                        const std::string& addressee,
                                        buzz::XmlElement* iq_body);

  IqRequest() {}
  virtual ~IqRequest() {}

  // Sends Iq stanza. Takes ownership of |stanza|.
  virtual void SendIq(buzz::XmlElement* stanza) = 0;

  // Sets callback that is called when reply stanza is received.
  virtual void set_callback(const ReplyCallback& callback) = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);

  DISALLOW_COPY_AND_ASSIGN(IqRequest);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_IQ_REQUEST_H_
