// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JAVASCRIPT_SIGNAL_STRATEGY_H_
#define REMOTING_JINGLE_GLUE_JAVASCRIPT_SIGNAL_STRATEGY_H_

#include "remoting/jingle_glue/signal_strategy.h"

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/jingle_glue/iq_request.h"

namespace remoting {

class XmppProxy;

class JavascriptSignalStrategy : public SignalStrategy {
 public:
  explicit JavascriptSignalStrategy(const std::string& your_jid);
  virtual ~JavascriptSignalStrategy();

  void AttachXmppProxy(scoped_refptr<XmppProxy> xmpp_proxy);

  // SignalStrategy interface.
  virtual void Init(StatusObserver* observer) OVERRIDE;
  virtual void StartSession(cricket::SessionManager* session_manager) OVERRIDE;
  virtual void EndSession() OVERRIDE;
  virtual JavascriptIqRequest* CreateIqRequest() OVERRIDE;

 private:
  std::string your_jid_;
  scoped_refptr<XmppProxy> xmpp_proxy_;
  JavascriptIqRegistry iq_registry_;
  scoped_ptr<SessionStartRequest> session_start_request_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JAVASCRIPT_SIGNAL_STRATEGY_H_
