// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

class MockSignalStrategy : public SignalStrategy {
 public:
  MockSignalStrategy();
  ~MockSignalStrategy() override;

  MOCK_METHOD0(Connect, void());
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD0(GetState, State());
  MOCK_CONST_METHOD0(GetError, Error());
  MOCK_CONST_METHOD0(GetLocalJid, std::string());
  MOCK_METHOD1(AddListener, void(Listener* listener));
  MOCK_METHOD1(RemoveListener, void(Listener* listener));
  MOCK_METHOD0(GetNextId, std::string());

  // GMock currently doesn't support move-only arguments, so we have
  // to use this hack here.
  MOCK_METHOD1(SendStanzaPtr, bool(buzz::XmlElement* stanza));
  bool SendStanza(std::unique_ptr<buzz::XmlElement> stanza) override {
    return SendStanzaPtr(stanza.release());
  }
};

}  // namespace remoting
