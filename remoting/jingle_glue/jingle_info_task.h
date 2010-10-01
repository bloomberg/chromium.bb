// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JINGLE_INFO_TASK_H_
#define REMOTING_JINGLE_GLUE_JINGLE_INFO_TASK_H_

#include <vector>
#include <string>

#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "third_party/libjingle/source/talk/p2p/client/httpportallocator.h"
#include "third_party/libjingle/source/talk/xmpp/xmppengine.h"
#include "third_party/libjingle/source/talk/xmpp/xmpptask.h"

namespace remoting {

// JingleInfoTask is used to discover addresses of jingle servers.
// See http://code.google.com/apis/talk/jep_extensions/jingleinfo.html
// for more details about the protocol.
//
// This is a copy of googleclient/talk/app/jingleinfotask.h .
class JingleInfoTask : public buzz::XmppTask {
 public:
  explicit JingleInfoTask(talk_base::TaskParent* parent)
      : XmppTask(parent, buzz::XmppEngine::HL_TYPE) {
  }

  virtual int ProcessStart();
  void RefreshJingleInfoNow();

  sigslot::signal3<const std::string&,
                   const std::vector<std::string>&,
                   const std::vector<talk_base::SocketAddress>&>
      SignalJingleInfo;

 protected:
  class JingleInfoGetTask;
  friend class JingleInfoGetTask;

  virtual bool HandleStanza(const buzz::XmlElement* stanza);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JINGLE_INFO_TASK_H_
