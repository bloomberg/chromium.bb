// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_IQ_REQUEST_H_
#define REMOTING_JINGLE_GLUE_IQ_REQUEST_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/weak_ptr.h"
#include "remoting/jingle_glue/xmpp_proxy.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "third_party/libjingle/source/talk/xmpp/xmppengine.h"

class MessageLoop;
class Task;

namespace buzz {
class XmppClient;
}  // namespace buzz

namespace cricket {
class SessionManager;
}  // namespace cricket

namespace talk_base {
class SocketAddress;
}  // namespace talk_base

namespace remoting {

// IqRequest class can be used to send an IQ stanza and then receive reply
// stanza for that request. It sends outgoing stanza when SendIq() is called,
// after that it forwards incoming reply stanza to the callback set with
// set_callback(). If each call to SendIq() will yield one invocation of the
// callback with the response.
class IqRequest {
 public:
  typedef Callback1<const buzz::XmlElement*>::Type ReplyCallback;

  IqRequest() {}
  virtual ~IqRequest() {}

  // Sends stanza of type |type| to |addressee|. |iq_body| contains body of
  // the stanza. Ownership of |iq_body| is transfered to IqRequest. Must
  // be called on the jingle thread.
  virtual void SendIq(const std::string& type, const std::string& addressee,
                      buzz::XmlElement* iq_body) = 0;

  // Sets callback that is called when reply stanza is received. Callback
  // is called on the jingle thread.
  virtual void set_callback(ReplyCallback* callback) = 0;

 protected:
  static buzz::XmlElement* MakeIqStanza(const std::string& type,
                                        const std::string& addressee,
                                        buzz::XmlElement* iq_body,
                                        const std::string& id);

 private:
  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);

  DISALLOW_COPY_AND_ASSIGN(IqRequest);
};

class JavascriptIqRequest;

class JavascriptIqRegistry : public XmppProxy::ResponseCallback {
 public:
  JavascriptIqRegistry();
  virtual ~JavascriptIqRegistry();

  // Dispatches the response to the IqRequest callback immediately.
  //
  // Does not take ownership of stanza.
  void DispatchResponse(buzz::XmlElement* stanza);

  // Registers |request|, returning the request ID used.
  std::string RegisterRequest(JavascriptIqRequest* request);

  // Removes all entries in the registry that refer to |request|.  Useful when
  // |request| is about to be destructed.
  void RemoveAllRequests(JavascriptIqRequest* request);

  void SetDefaultHandler(JavascriptIqRequest* default_request);

 private:
  typedef std::map<std::string, JavascriptIqRequest*> IqRequestMap;

  // XmppProxy::ResponseCallback interface.
  virtual void OnIq(const std::string& response_xml);

  IqRequestMap requests_;
  int current_id_;
  JavascriptIqRequest* default_handler_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptIqRegistry);
};

// This call must only be used on the thread it was created on.
class JavascriptIqRequest : public IqRequest {
 public:
  JavascriptIqRequest(JavascriptIqRegistry* registry,
                      scoped_refptr<XmppProxy> xmpp_proxy);
  virtual ~JavascriptIqRequest();

  virtual void SendIq(const std::string& type, const std::string& addressee,
                      buzz::XmlElement* iq_body);

  // Similar to SendIq(), but has 3 major differences:
  //
  //   (1) It does absoluately no error checking. Caller is responsible for
  //       validity.
  //   (2) It doesn't add an Iq envelope. Caller is again responsible.
  //   (3) BecomeDefaultHandler() must have been called.
  //
  // TODO(ajwong): We need to rationalize the semantics of these two APIs.
  // SendRawIq() is a hack for SessionStartRequest which uses a different memory
  // management convention.
  void SendRawIq(buzz::XmlElement* stanza);

  // This function is a hack to support SessionStartRequest. It registers the
  // current JavascriptIqRequest instance to be the single passthrough filter
  // for the associated JavascriptIqRegistry.  What this means is that any IQ
  // packet that does not match a previous respone packet will be funneled
  // through to this JavascriptIqRegistry instance (basically a default
  // handler).  It also means that the registry will not be tracking any of the
  // packets sent from this JavascriptIqRegistry instance.
  //
  // TODO(ajwong): We need to take a high-level look at IqRequest to understand
  // how to make this API cleaner.
  virtual void BecomeDefaultHandler();

  virtual void set_callback(ReplyCallback* callback);

 private:
  friend class JavascriptIqRegistry;

  scoped_ptr<ReplyCallback> callback_;
  scoped_refptr<XmppProxy> xmpp_proxy_;
  JavascriptIqRegistry* registry_;
  bool is_default_handler_;

  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);
};

class XmppIqRequest : public IqRequest, private buzz::XmppIqHandler {
 public:
  typedef Callback1<const buzz::XmlElement*>::Type ReplyCallback;

  XmppIqRequest(MessageLoop* message_loop, buzz::XmppClient* xmpp_client);
  virtual ~XmppIqRequest();

  virtual void SendIq(const std::string& type, const std::string& addressee,
                      buzz::XmlElement* iq_body);
  virtual void set_callback(ReplyCallback* callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);

  // XmppIqHandler interface.
  virtual void IqResponse(buzz::XmppIqCookie cookie,
                          const buzz::XmlElement* stanza);

  void Unregister();

  // TODO(ajwong): This used to hold a reference to the jingle client...make
  // sure the lifetime names sense now.
  MessageLoop* message_loop_;
  buzz::XmppClient* xmpp_client_;
  buzz::XmppIqCookie cookie_;
  scoped_ptr<ReplyCallback> callback_;
};

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
// TODO(ajwong): Move to another file.
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

// This class handles proxying the Jingle establishment messages between the
// client and the server when proxying XMPP through Javascript.
//
// The |request| object is used to send and receive IQ stanzas from the XMPP
// network.  The |session_manager| controls sending and receiving of stanzas
// after Run() is invoked.
//
// This class is not threadsafe, and should only be used on the thread it is
// created on.
//
// TODO(ajwong): Move into its own file, and rename to something better since
// this is not actually a Request. Maybe SessionEstablishmentConnector.
class SessionStartRequest : public sigslot::has_slots<> {
 public:
  SessionStartRequest(JavascriptIqRequest* request,
                      cricket::SessionManager* session_manager);
  ~SessionStartRequest();

  void Run();

 private:
  void OnResponse(const buzz::XmlElement* stanza);

  void OnOutgoingMessage(cricket::SessionManager* manager,
                         const buzz::XmlElement* stanza);

  scoped_ptr<JavascriptIqRequest> request_;
  cricket::SessionManager* session_manager_;

  DISALLOW_COPY_AND_ASSIGN(SessionStartRequest);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_IQ_REQUEST_H_
