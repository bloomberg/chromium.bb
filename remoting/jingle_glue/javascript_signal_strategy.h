// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JAVASCRIPT_SIGNAL_STRATEGY_H_
#define REMOTING_JINGLE_GLUE_JAVASCRIPT_SIGNAL_STRATEGY_H_

#include "remoting/jingle_glue/signal_strategy.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/jingle_glue/xmpp_proxy.h"

namespace remoting {

class XmppProxy;

class JavascriptSignalStrategy : public SignalStrategy,
                                 public XmppProxy::ResponseCallback,
                                 public base::NonThreadSafe {
 public:
  explicit JavascriptSignalStrategy(const std::string& local_jid);
  virtual ~JavascriptSignalStrategy();

  void AttachXmppProxy(scoped_refptr<XmppProxy> xmpp_proxy);

  // SignalStrategy interface.
  virtual void Connect() OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual State GetState() const OVERRIDE;
  virtual Error GetError() const OVERRIDE;
  virtual std::string GetLocalJid() const OVERRIDE;
  virtual void AddListener(Listener* listener) OVERRIDE;
  virtual void RemoveListener(Listener* listener) OVERRIDE;
  virtual bool SendStanza(scoped_ptr<buzz::XmlElement> stanza) OVERRIDE;
  virtual std::string GetNextId() OVERRIDE;

  // XmppProxy::ResponseCallback interface.
  virtual void OnIq(const std::string& stanza) OVERRIDE;

 private:
  std::string local_jid_;
  scoped_refptr<XmppProxy> xmpp_proxy_;

  ObserverList<Listener> listeners_;

  int last_id_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JAVASCRIPT_SIGNAL_STRATEGY_H_
