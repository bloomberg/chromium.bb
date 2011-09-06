// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JAVASCRIPT_IQ_REQUEST_H_
#define REMOTING_JINGLE_GLUE_JAVASCRIPT_IQ_REQUEST_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/jingle_glue/iq_request.h"

namespace remoting {

class JavascriptIqRequest;
class SignalStrategy;

class JavascriptIqRegistry {
 public:
  JavascriptIqRegistry();
  virtual ~JavascriptIqRegistry();

  // Dispatches the response to the IqRequest callback immediately.
  //
  // Does not take ownership of stanza.
  void DispatchResponse(buzz::XmlElement* stanza);

  // Registers |request| with the specified |id|.
  void RegisterRequest(JavascriptIqRequest* request, const std::string& id);

  // Removes all entries in the registry that refer to |request|.  Useful when
  // |request| is about to be destructed.
  void RemoveAllRequests(JavascriptIqRequest* request);

  void SetDefaultHandler(JavascriptIqRequest* default_request);

  // Called by JavascriptSignalStrategy.
  void OnIncomingStanza(const buzz::XmlElement* stanza);

 private:
  typedef std::map<std::string, JavascriptIqRequest*> IqRequestMap;

  IqRequestMap requests_;
  JavascriptIqRequest* default_handler_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptIqRegistry);
};

// This call must only be used on the thread it was created on.
class JavascriptIqRequest : public IqRequest {
 public:
  JavascriptIqRequest(SignalStrategy* signal_strategy,
                      JavascriptIqRegistry* registry);
  virtual ~JavascriptIqRequest();

  // IqRequest interface.
  virtual void SendIq(buzz::XmlElement* iq_body) OVERRIDE;
  virtual void set_callback(const ReplyCallback& callback) OVERRIDE;

 private:
  friend class JavascriptIqRegistry;

  ReplyCallback callback_;
  SignalStrategy* signal_strategy_;
  JavascriptIqRegistry* registry_;

  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JAVASCRIPT_IQ_REQUEST_H_
