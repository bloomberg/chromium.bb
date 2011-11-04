// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockSignalStrategy : public SignalStrategy {
 public:
  MockSignalStrategy();
  virtual ~MockSignalStrategy();

  MOCK_METHOD1(Init, void(StatusObserver*));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(AddListener, void(Listener* listener));
  MOCK_METHOD1(RemoveListener, void(Listener* listener));
  MOCK_METHOD1(SendStanza, bool(buzz::XmlElement* stanza));
  MOCK_METHOD0(GetNextId, std::string());
  MOCK_METHOD0(CreateIqRequest, IqRequest*());
};

}  // namespace remoting
