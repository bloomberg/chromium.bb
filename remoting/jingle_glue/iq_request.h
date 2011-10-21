// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_IQ_REQUEST_H_
#define REMOTING_JINGLE_GLUE_IQ_REQUEST_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/jingle_glue/iq_request.h"

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {

class IqRequest;
class SignalStrategy;

// IqRegistry handles routing of iq responses to corresponding
// IqRequest objects. Created in SignalStrategy and should not be used
// directly.
//
// TODO(sergeyu): Separate IqRegistry and IqRequest from
// SignalStrategy and remove CreateIqRequest() method from SignalStrategy.
class IqRegistry {
 public:
  IqRegistry();
  ~IqRegistry();

  // Dispatches the response to the IqRequest callback immediately.
  //
  // Does not take ownership of stanza.
  void DispatchResponse(buzz::XmlElement* stanza);

  // Registers |request| with the specified |id|.
  void RegisterRequest(IqRequest* request, const std::string& id);

  // Removes all entries in the registry that refer to |request|.
  void RemoveAllRequests(IqRequest* request);

  void SetDefaultHandler(IqRequest* default_request);

  // Called by SignalStrategy implementation. Returns true if the
  // stanza was handled and should not be processed further.
  bool OnIncomingStanza(const buzz::XmlElement* stanza);

 private:
  typedef std::map<std::string, IqRequest*> IqRequestMap;

  IqRequestMap requests_;

  DISALLOW_COPY_AND_ASSIGN(IqRegistry);
};

// This call must only be used on the thread it was created on.
class IqRequest {
 public:
  typedef base::Callback<void(const buzz::XmlElement*)> ReplyCallback;

  static buzz::XmlElement* MakeIqStanza(const std::string& type,
                                        const std::string& addressee,
                                        buzz::XmlElement* iq_body);

  IqRequest(); // Should be used for tests only.
  IqRequest(SignalStrategy* signal_strategy, IqRegistry* registry);
  virtual ~IqRequest();

  // Sends Iq stanza. Takes ownership of |stanza|.
  virtual void SendIq(buzz::XmlElement* stanza);

  // Sets callback that is called when reply stanza is received.
  virtual void set_callback(const ReplyCallback& callback);

 private:
  friend class IqRegistry;
  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);

  ReplyCallback callback_;
  SignalStrategy* signal_strategy_;
  IqRegistry* registry_;

  DISALLOW_COPY_AND_ASSIGN(IqRequest);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_IQ_REQUEST_H_
