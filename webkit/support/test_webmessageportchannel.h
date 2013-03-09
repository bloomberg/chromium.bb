// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_TEST_WEBMESSAGEPORTCHANNEL_H_
#define WEBKIT_SUPPORT_TEST_WEBMESSAGEPORTCHANNEL_H_

#include <queue>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMessagePortChannel.h"

namespace WebKit {
class WebString;
}

class TestWebMessagePortChannel
    : public WebKit::WebMessagePortChannel,
      public base::RefCounted<TestWebMessagePortChannel> {
 public:
  TestWebMessagePortChannel();

  // WebMessagePortChannel implementation.
  virtual void setClient(WebKit::WebMessagePortChannelClient*) OVERRIDE;
  virtual void destroy() OVERRIDE;
  // WebKit versions of WebCore::MessagePortChannel.
  virtual void entangle(WebKit::WebMessagePortChannel*) OVERRIDE;
  // Callee receives ownership of the passed vector.
  virtual void postMessage(const WebKit::WebString&,
                           WebKit::WebMessagePortChannelArray*) OVERRIDE;
  virtual bool tryGetMessage(WebKit::WebString*,
                             WebKit::WebMessagePortChannelArray&) OVERRIDE;

 protected:
  virtual ~TestWebMessagePortChannel();

 private:
  friend class base::RefCounted<TestWebMessagePortChannel>;

  class Message;

  void queueMessage(Message*);

  WebKit::WebMessagePortChannelClient* client_;
  scoped_refptr<TestWebMessagePortChannel> remote_;
  std::queue<Message*> message_queue_;

  DISALLOW_COPY_AND_ASSIGN(TestWebMessagePortChannel);
};

#endif
