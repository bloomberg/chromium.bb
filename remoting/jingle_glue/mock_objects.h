// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockSignalStrategy : public SignalStrategy {
 public:
  MockSignalStrategy();
  virtual ~MockSignalStrategy();

  MOCK_METHOD1(Init, void(StatusObserver*));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(SetListener, void(Listener* listener));
  MOCK_METHOD1(SendStanza, void(buzz::XmlElement* stanza));
  MOCK_METHOD0(GetNextId, std::string());
  MOCK_METHOD0(CreateIqRequest, IqRequest*());
};

class MockIqRequest : public IqRequest {
 public:
  MockIqRequest();
  virtual ~MockIqRequest();

  MOCK_METHOD1(SendIq, void(buzz::XmlElement* stanza));
  MOCK_METHOD1(set_callback, void(const IqRequest::ReplyCallback&));

  // Ensure this takes ownership of the pointer, as the real IqRequest object
  // would, to avoid memory-leak.
  void set_callback_hook(const IqRequest::ReplyCallback& callback) {
    callback_ = callback;
  }

  void Init() {
    ON_CALL(*this, set_callback(testing::_))
        .WillByDefault(testing::Invoke(
            this, &MockIqRequest::set_callback_hook));
  }

  ReplyCallback& callback() { return callback_; }

 private:
  ReplyCallback callback_;
};

}  // namespace remoting
