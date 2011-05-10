// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockSignalStrategy : public SignalStrategy {
 public:
  MockSignalStrategy();
  virtual ~MockSignalStrategy();

  MOCK_METHOD1(Init, void(StatusObserver*));
  MOCK_METHOD0(port_allocator, cricket::BasicPortAllocator*());
  MOCK_METHOD2(ConfigureAllocator, void(cricket::HttpPortAllocator*, Task*));
  MOCK_METHOD1(StartSession, void(cricket::SessionManager*));
  MOCK_METHOD0(EndSession, void());
  MOCK_METHOD0(CreateIqRequest, IqRequest*());
};

class MockIqRequest : public IqRequest {
 public:
  MockIqRequest();
  virtual ~MockIqRequest();

  MOCK_METHOD3(SendIq, void(const std::string& type,
                            const std::string& addressee,
                            buzz::XmlElement* iq_body));
  MOCK_METHOD1(set_callback, void(IqRequest::ReplyCallback*));

  // Ensure this takes ownership of the pointer, as the real IqRequest object
  // would, to avoid memory-leak.
  void set_callback_hook(IqRequest::ReplyCallback* callback) {
    callback_.reset(callback);
  }

  void Init() {
    ON_CALL(*this, set_callback(testing::_))
        .WillByDefault(testing::Invoke(
            this, &MockIqRequest::set_callback_hook));
  }

  IqRequest::ReplyCallback* callback() { return callback_.get(); }

 private:
  scoped_ptr<IqRequest::ReplyCallback> callback_;
};

}  // namespace remoting
