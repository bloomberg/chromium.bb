// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_policy.h"
#include "net/base/cookie_store.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"
#include "net/socket_stream/socket_stream.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_job.h"
#include "net/websockets/websocket_throttle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

namespace net {

class MockSocketStream : public SocketStream {
 public:
  MockSocketStream(const GURL& url, SocketStream::Delegate* delegate)
      : SocketStream(url, delegate) {}
  virtual ~MockSocketStream() {}

  virtual void Connect() {}
  virtual bool SendData(const char* data, int len) {
    sent_data_ += std::string(data, len);
    return true;
  }

  virtual void Close() {}
  virtual void RestartWithAuth(
      const string16& username, const string16& password) {}
  virtual void DetachDelegate() {
    delegate_ = NULL;
  }

  const std::string& sent_data() const {
    return sent_data_;
  }

 private:
  std::string sent_data_;
};

class MockSocketStreamDelegate : public SocketStream::Delegate {
 public:
  MockSocketStreamDelegate()
      : amount_sent_(0) {}
  virtual ~MockSocketStreamDelegate() {}

  virtual void OnConnected(SocketStream* socket, int max_pending_send_allowed) {
  }
  virtual void OnSentData(SocketStream* socket, int amount_sent) {
    amount_sent_ += amount_sent;
  }
  virtual void OnReceivedData(SocketStream* socket,
                              const char* data, int len) {
    received_data_ += std::string(data, len);
  }
  virtual void OnClose(SocketStream* socket) {
  }

  size_t amount_sent() const { return amount_sent_; }
  const std::string& received_data() const { return received_data_; }

 private:
  int amount_sent_;
  std::string received_data_;
};

class MockCookieStore : public CookieStore {
 public:
  struct Entry {
    GURL url;
    std::string cookie_line;
    CookieOptions options;
  };
  MockCookieStore() {}

  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const CookieOptions& options) {
    Entry entry;
    entry.url = url;
    entry.cookie_line = cookie_line;
    entry.options = options;
    entries_.push_back(entry);
    return true;
  }
  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const CookieOptions& options) {
    std::string result;
    for (size_t i = 0; i < entries_.size(); i++) {
      Entry &entry = entries_[i];
      if (url == entry.url) {
        if (!result.empty()) {
          result += "; ";
        }
        result += entry.cookie_line;
      }
    }
    return result;
  }
  virtual void DeleteCookie(const GURL& url,
                            const std::string& cookie_name) {}
  virtual CookieMonster* GetCookieMonster() { return NULL; }

  const std::vector<Entry>& entries() const { return entries_; }

 private:
  friend class base::RefCountedThreadSafe<MockCookieStore>;
  virtual ~MockCookieStore() {}

  std::vector<Entry> entries_;
};

class MockCookiePolicy : public CookiePolicy,
                         public base::RefCountedThreadSafe<MockCookiePolicy> {
 public:
  MockCookiePolicy() : allow_all_cookies_(true), callback_(NULL) {}

  void set_allow_all_cookies(bool allow_all_cookies) {
    allow_all_cookies_ = allow_all_cookies;
  }

  virtual int CanGetCookies(const GURL& url,
                            const GURL& first_party_for_cookies,
                            CompletionCallback* callback) {
    DCHECK(!callback_);
    callback_ = callback;
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &MockCookiePolicy::OnCanGetCookies));
    return ERR_IO_PENDING;
  }

  virtual int CanSetCookie(const GURL& url,
                           const GURL& first_party_for_cookies,
                           const std::string& cookie_line,
                           CompletionCallback* callback) {
    DCHECK(!callback_);
    callback_ = callback;
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &MockCookiePolicy::OnCanSetCookie));
    return ERR_IO_PENDING;
  }

 private:
  friend class base::RefCountedThreadSafe<MockCookiePolicy>;
  virtual ~MockCookiePolicy() {}

  void OnCanGetCookies() {
    CompletionCallback* callback = callback_;
    callback_ = NULL;
    if (allow_all_cookies_)
      callback->Run(OK);
    else
      callback->Run(ERR_ACCESS_DENIED);
  }
  void OnCanSetCookie() {
    CompletionCallback* callback = callback_;
    callback_ = NULL;
    if (allow_all_cookies_)
      callback->Run(OK);
    else
      callback->Run(ERR_ACCESS_DENIED);
  }

  bool allow_all_cookies_;
  CompletionCallback* callback_;
};

class MockURLRequestContext : public URLRequestContext {
 public:
  MockURLRequestContext(CookieStore* cookie_store,
                        CookiePolicy* cookie_policy) {
    set_cookie_store(cookie_store);
    set_cookie_policy(cookie_policy);
  }

 private:
  friend class base::RefCountedThreadSafe<MockURLRequestContext>;
  virtual ~MockURLRequestContext() {}
};

class WebSocketJobTest : public PlatformTest {
 public:
  virtual void SetUp() {
    cookie_store_ = new MockCookieStore;
    cookie_policy_ = new MockCookiePolicy;
    context_ = new MockURLRequestContext(
        cookie_store_.get(), cookie_policy_.get());
  }
  virtual void TearDown() {
    cookie_store_ = NULL;
    cookie_policy_ = NULL;
    context_ = NULL;
    websocket_ = NULL;
    socket_ = NULL;
  }
 protected:
  void InitWebSocketJob(const GURL& url, MockSocketStreamDelegate* delegate) {
    websocket_ = new WebSocketJob(delegate);
    socket_ = new MockSocketStream(url, websocket_.get());
    websocket_->InitSocketStream(socket_.get());
    websocket_->set_context(context_.get());
    websocket_->state_ = WebSocketJob::CONNECTING;
    struct addrinfo addr;
    memset(&addr, 0, sizeof(struct addrinfo));
    addr.ai_family = AF_INET;
    addr.ai_addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in sa_in;
    memset(&sa_in, 0, sizeof(struct sockaddr_in));
    memcpy(&sa_in.sin_addr, "\x7f\0\0\1", 4);
    addr.ai_addr = reinterpret_cast<sockaddr*>(&sa_in);
    addr.ai_next = NULL;
    websocket_->addresses_.Copy(&addr, true);
    WebSocketThrottle::GetInstance()->PutInQueue(websocket_);
  }
  WebSocketJob::State GetWebSocketJobState() {
    return websocket_->state_;
  }
  void CloseWebSocketJob() {
    if (websocket_->socket_) {
      websocket_->socket_->DetachDelegate();
      WebSocketThrottle::GetInstance()->RemoveFromQueue(websocket_);
    }
    websocket_->state_ = WebSocketJob::CLOSED;
    websocket_->delegate_ = NULL;
    websocket_->socket_ = NULL;
  }

  scoped_refptr<MockCookieStore> cookie_store_;
  scoped_refptr<MockCookiePolicy> cookie_policy_;
  scoped_refptr<MockURLRequestContext> context_;
  scoped_refptr<WebSocketJob> websocket_;
  scoped_refptr<MockSocketStream> socket_;
};

TEST_F(WebSocketJobTest, SimpleHandshake) {
  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate);

  static const char* kHandshakeRequestMessage =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "\r\n"
      "^n:ds[4U";

  bool sent = websocket_->SendData(kHandshakeRequestMessage,
                                   strlen(kHandshakeRequestMessage));
  EXPECT_TRUE(sent);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeRequestMessage, socket_->sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_.get(), strlen(kHandshakeRequestMessage));
  EXPECT_EQ(strlen(kHandshakeRequestMessage), delegate.amount_sent());

  const char kHandshakeResponseMessage[] =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  websocket_->OnReceivedData(socket_.get(),
                             kHandshakeResponseMessage,
                             strlen(kHandshakeResponseMessage));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeResponseMessage, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());
  CloseWebSocketJob();
}

TEST_F(WebSocketJobTest, SlowHandshake) {
  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate);

  static const char* kHandshakeRequestMessage =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "\r\n"
      "^n:ds[4U";

  bool sent = websocket_->SendData(kHandshakeRequestMessage,
                                   strlen(kHandshakeRequestMessage));
  EXPECT_TRUE(sent);
  // We assume request is sent in one data chunk (from WebKit)
  // We don't support streaming request.
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeRequestMessage, socket_->sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_.get(), strlen(kHandshakeRequestMessage));
  EXPECT_EQ(strlen(kHandshakeRequestMessage), delegate.amount_sent());

  const char kHandshakeResponseMessage[] =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  std::vector<std::string> lines;
  base::SplitString(kHandshakeResponseMessage, '\n', &lines);
  for (size_t i = 0; i < lines.size() - 2; i++) {
    std::string line = lines[i] + "\r\n";
    SCOPED_TRACE("Line: " + line);
    websocket_->OnReceivedData(socket_,
                               line.c_str(),
                               line.size());
    MessageLoop::current()->RunAllPending();
    EXPECT_TRUE(delegate.received_data().empty());
    EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  }
  websocket_->OnReceivedData(socket_.get(), "\r\n", 2);
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(delegate.received_data().empty());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnReceivedData(socket_.get(), "8jKS'y:G*Co,Wxa-", 16);
  EXPECT_EQ(kHandshakeResponseMessage, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());
  CloseWebSocketJob();
}

TEST_F(WebSocketJobTest, HandshakeWithCookie) {
  GURL url("ws://example.com/demo");
  GURL cookieUrl("http://example.com/demo");
  CookieOptions cookie_options;
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test=1", cookie_options);
  cookie_options.set_include_httponly();
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test-httponly=1", cookie_options);

  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate);

  static const char* kHandshakeRequestMessage =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "Cookie: WK-test=1\r\n"
      "\r\n"
      "^n:ds[4U";

  static const char* kHandshakeRequestExpected =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "Cookie: CR-test=1; CR-test-httponly=1\r\n"
      "\r\n"
      "^n:ds[4U";

  bool sent = websocket_->SendData(kHandshakeRequestMessage,
                                   strlen(kHandshakeRequestMessage));
  EXPECT_TRUE(sent);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeRequestExpected, socket_->sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_, strlen(kHandshakeRequestExpected));
  EXPECT_EQ(strlen(kHandshakeRequestMessage), delegate.amount_sent());

  const char kHandshakeResponseMessage[] =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Set-Cookie: CR-set-test=1\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  static const char* kHandshakeResponseExpected =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  websocket_->OnReceivedData(socket_.get(),
                             kHandshakeResponseMessage,
                             strlen(kHandshakeResponseMessage));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeResponseExpected, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());

  EXPECT_EQ(3U, cookie_store_->entries().size());
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[0].url);
  EXPECT_EQ("CR-test=1", cookie_store_->entries()[0].cookie_line);
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[1].url);
  EXPECT_EQ("CR-test-httponly=1", cookie_store_->entries()[1].cookie_line);
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[2].url);
  EXPECT_EQ("CR-set-test=1", cookie_store_->entries()[2].cookie_line);

  CloseWebSocketJob();
}

TEST_F(WebSocketJobTest, HandshakeWithCookieButNotAllowed) {
  GURL url("ws://example.com/demo");
  GURL cookieUrl("http://example.com/demo");
  CookieOptions cookie_options;
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test=1", cookie_options);
  cookie_options.set_include_httponly();
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test-httponly=1", cookie_options);
  cookie_policy_->set_allow_all_cookies(false);

  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate);

  static const char* kHandshakeRequestMessage =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "Cookie: WK-test=1\r\n"
      "\r\n"
      "^n:ds[4U";

  static const char* kHandshakeRequestExpected =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
      "Origin: http://example.com\r\n"
      "\r\n"
      "^n:ds[4U";

  bool sent = websocket_->SendData(kHandshakeRequestMessage,
                                   strlen(kHandshakeRequestMessage));
  EXPECT_TRUE(sent);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeRequestExpected, socket_->sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_, strlen(kHandshakeRequestExpected));
  EXPECT_EQ(strlen(kHandshakeRequestMessage), delegate.amount_sent());

  const char kHandshakeResponseMessage[] =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Set-Cookie: CR-set-test=1\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  static const char* kHandshakeResponseExpected =
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";

  websocket_->OnReceivedData(socket_.get(),
                             kHandshakeResponseMessage,
                             strlen(kHandshakeResponseMessage));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeResponseExpected, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());

  EXPECT_EQ(2U, cookie_store_->entries().size());
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[0].url);
  EXPECT_EQ("CR-test=1", cookie_store_->entries()[0].cookie_line);
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[1].url);
  EXPECT_EQ("CR-test-httponly=1", cookie_store_->entries()[1].cookie_line);

  CloseWebSocketJob();
}

}  // namespace net
