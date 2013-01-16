// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/test_webmessageportchannel.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMessagePortChannelClient.h"

using WebKit::WebMessagePortChannel;
using WebKit::WebMessagePortChannelArray;
using WebKit::WebMessagePortChannelClient;
using WebKit::WebString;

class TestWebMessagePortChannel::Message {
 public:
  Message(WebString data, WebMessagePortChannelArray* ports)
      : data_(data),
        ports_(ports) {
  }
  ~Message() {
    if (ports_ == NULL)
      return;
    for (size_t i = 0; i < ports_->size(); ++i)
      (*ports_)[i]->destroy();
  }
  WebString data() const { return data_; }
  WebMessagePortChannelArray* ports() { return ports_.get(); }

 private:
  WebString data_;
  scoped_ptr<WebMessagePortChannelArray> ports_;
  DISALLOW_COPY_AND_ASSIGN(Message);
};

TestWebMessagePortChannel::TestWebMessagePortChannel()
    : client_(NULL) {
  AddRef();
}

void TestWebMessagePortChannel::setClient(WebMessagePortChannelClient* client) {
  client_ = client;
}

void TestWebMessagePortChannel::destroy() {
  while (!message_queue_.empty()) {
    delete message_queue_.front();
    message_queue_.pop();
  }
  Release();
}

void TestWebMessagePortChannel::entangle(WebMessagePortChannel* remote) {
  remote_ = static_cast<TestWebMessagePortChannel*>(remote);
}

void TestWebMessagePortChannel::postMessage(const WebString& data,
    WebMessagePortChannelArray* ports) {
  if (remote_ == NULL)
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TestWebMessagePortChannel::queueMessage, remote_.get(),
                 new Message(data, ports)));
}

bool TestWebMessagePortChannel::tryGetMessage(WebString* data,
    WebMessagePortChannelArray& ports) {
  if (message_queue_.empty())
    return false;
  scoped_ptr<Message> message(message_queue_.front());
  message_queue_.pop();
  *data = message->data();
  if (WebMessagePortChannelArray* message_ports = message->ports())
    ports.swap(*message_ports);
  return true;
}

TestWebMessagePortChannel::~TestWebMessagePortChannel() {}

void TestWebMessagePortChannel::queueMessage(Message* message) {
  bool was_empty = message_queue_.empty();
  message_queue_.push(message);
  if (client_ && was_empty)
    client_->messageAvailable();
}
