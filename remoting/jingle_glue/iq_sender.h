// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_IQ_SENDER_H_
#define REMOTING_JINGLE_GLUE_IQ_SENDER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/jingle_glue/signal_strategy.h"

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {

class IqRequest;
class SignalStrategy;

// IqSender handles sending iq requests and routing of responses to
// those requests.
class IqSender : public SignalStrategy::Listener {
 public:
  typedef base::Callback<void(const buzz::XmlElement*)> ReplyCallback;

  explicit IqSender(SignalStrategy* signal_strategy);
  virtual ~IqSender();

  // Send an iq stanza. Returns an IqRequest object that represends
  // the request. |callback| is called when response to |stanza| is
  // received. Destroy the returned IqRequest to cancel the callback.
  // Takes ownership of |stanza|. Caller must take ownership of the
  // result. Result must be destroyed before sender is destroyed.
  IqRequest* SendIq(buzz::XmlElement* stanza,
                    const ReplyCallback& callback) WARN_UNUSED_RESULT;

  // Same as above, but also formats the message. Takes ownership of
  // |iq_body|.
  IqRequest* SendIq(const std::string& type,
                    const std::string& addressee,
                    buzz::XmlElement* iq_body,
                    const ReplyCallback& callback) WARN_UNUSED_RESULT;

  // SignalStrategy::Listener implementation.
  virtual bool OnIncomingStanza(const buzz::XmlElement* stanza) OVERRIDE;

 private:
  typedef std::map<std::string, IqRequest*> IqRequestMap;
  friend class IqRequest;

  // Helper function used to create iq stanzas.
  static buzz::XmlElement* MakeIqStanza(const std::string& type,
                                        const std::string& addressee,
                                        buzz::XmlElement* iq_body);

  // Removes |request| from the list of pending requests. Called by IqRequest.
  void RemoveRequest(IqRequest* request);

  SignalStrategy* signal_strategy_;
  IqRequestMap requests_;

  DISALLOW_COPY_AND_ASSIGN(IqSender);
};

// This call must only be used on the thread it was created on.
class IqRequest {
 public:
  IqRequest(IqSender* sender, const IqSender::ReplyCallback& callback);
  ~IqRequest();

 private:
  friend class IqSender;

  // Called by IqSender when a response is received.
  void OnResponse(const buzz::XmlElement* stanza);

  IqSender* sender_;
  IqSender::ReplyCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(IqRequest);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_IQ_SENDER_H_
