// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_UNITTEST_H_
#define NET_URL_REQUEST_URL_REQUEST_UNITTEST_H_
#pragma once

#include <stdlib.h>

#include <sstream>
#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_policy.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/disk_cache/disk_cache.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/url_util.h"

using base::TimeDelta;

//-----------------------------------------------------------------------------

class TestCookiePolicy : public net::CookiePolicy {
 public:
  enum Options {
    NO_GET_COOKIES = 1 << 0,
    NO_SET_COOKIE  = 1 << 1,
    ASYNC          = 1 << 2,
    FORCE_SESSION  = 1 << 3,
  };

  explicit TestCookiePolicy(int options_bit_mask)
      : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
        options_(options_bit_mask),
        callback_(NULL) {
  }

  virtual int CanGetCookies(const GURL& url, const GURL& first_party,
                            net::CompletionCallback* callback) {
    if ((options_ & ASYNC) && callback) {
      callback_ = callback;
      MessageLoop::current()->PostTask(FROM_HERE,
          method_factory_.NewRunnableMethod(
              &TestCookiePolicy::DoGetCookiesPolicy, url, first_party));
      return net::ERR_IO_PENDING;
    }

    if (options_ & NO_GET_COOKIES)
      return net::ERR_ACCESS_DENIED;

    return net::OK;
  }

  virtual int CanSetCookie(const GURL& url, const GURL& first_party,
                           const std::string& cookie_line,
                           net::CompletionCallback* callback) {
    if ((options_ & ASYNC) && callback) {
      callback_ = callback;
      MessageLoop::current()->PostTask(FROM_HERE,
          method_factory_.NewRunnableMethod(
              &TestCookiePolicy::DoSetCookiePolicy, url, first_party,
              cookie_line));
      return net::ERR_IO_PENDING;
    }

    if (options_ & NO_SET_COOKIE)
      return net::ERR_ACCESS_DENIED;

    if (options_ & FORCE_SESSION)
      return net::OK_FOR_SESSION_ONLY;

    return net::OK;
  }

 private:
  void DoGetCookiesPolicy(const GURL& url, const GURL& first_party) {
    int policy = CanGetCookies(url, first_party, NULL);

    DCHECK(callback_);
    net::CompletionCallback* callback = callback_;
    callback_ = NULL;
    callback->Run(policy);
  }

  void DoSetCookiePolicy(const GURL& url, const GURL& first_party,
                         const std::string& cookie_line) {
    int policy = CanSetCookie(url, first_party, cookie_line, NULL);

    DCHECK(callback_);
    net::CompletionCallback* callback = callback_;
    callback_ = NULL;
    callback->Run(policy);
  }

  ScopedRunnableMethodFactory<TestCookiePolicy> method_factory_;
  int options_;
  net::CompletionCallback* callback_;
};

//-----------------------------------------------------------------------------

class TestURLRequestContext : public net::URLRequestContext {
 public:
  TestURLRequestContext() {
    host_resolver_ =
        net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                      NULL, NULL);
    proxy_service_ = net::ProxyService::CreateDirect();
    Init();
  }

  explicit TestURLRequestContext(const std::string& proxy) {
    host_resolver_  =
        net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                      NULL, NULL);
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString(proxy);
    proxy_service_ = net::ProxyService::CreateFixed(proxy_config);
    Init();
  }

  void set_cookie_policy(net::CookiePolicy* policy) {
    cookie_policy_ = policy;
  }

 protected:
  virtual ~TestURLRequestContext() {
    delete ftp_transaction_factory_;
    delete http_transaction_factory_;
    delete http_auth_handler_factory_;
    delete cert_verifier_;
    delete host_resolver_;
  }

 private:
  void Init() {
    cert_verifier_ = new net::CertVerifier;
    ftp_transaction_factory_ = new net::FtpNetworkLayer(host_resolver_);
    ssl_config_service_ = new net::SSLConfigServiceDefaults;
    http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault(
        host_resolver_);
    http_transaction_factory_ = new net::HttpCache(
        net::HttpNetworkLayer::CreateFactory(host_resolver_,
                                             cert_verifier_,
                                             NULL /* dnsrr_resolver */,
                                             NULL /* dns_cert_checker */,
                                             NULL /* ssl_host_info_factory */,
                                             proxy_service_,
                                             ssl_config_service_,
                                             http_auth_handler_factory_,
                                             network_delegate_,
                                             NULL),
        NULL /* net_log */,
        net::HttpCache::DefaultBackend::InMemory(0));
    // In-memory cookie store.
    cookie_store_ = new net::CookieMonster(NULL, NULL);
    accept_language_ = "en-us,fr";
    accept_charset_ = "iso-8859-1,*,utf-8";
  }
};

//-----------------------------------------------------------------------------

class TestURLRequest : public net::URLRequest {
 public:
  TestURLRequest(const GURL& url, Delegate* delegate)
      : net::URLRequest(url, delegate) {
    set_context(new TestURLRequestContext());
  }
};

//-----------------------------------------------------------------------------

class TestDelegate : public net::URLRequest::Delegate {
 public:
  TestDelegate()
      : cancel_in_rr_(false),
        cancel_in_rs_(false),
        cancel_in_rd_(false),
        cancel_in_rd_pending_(false),
        cancel_in_getcookiesblocked_(false),
        cancel_in_setcookieblocked_(false),
        quit_on_complete_(true),
        quit_on_redirect_(false),
        allow_certificate_errors_(false),
        response_started_count_(0),
        received_bytes_count_(0),
        received_redirect_count_(0),
        blocked_get_cookies_count_(0),
        blocked_set_cookie_count_(0),
        set_cookie_count_(0),
        received_data_before_response_(false),
        request_failed_(false),
        have_certificate_errors_(false),
        buf_(new net::IOBuffer(kBufferSize)) {
  }

  virtual void OnReceivedRedirect(net::URLRequest* request, const GURL& new_url,
                                  bool* defer_redirect) {
    received_redirect_count_++;
    if (quit_on_redirect_) {
      *defer_redirect = true;
      MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    } else if (cancel_in_rr_) {
      request->Cancel();
    }
  }

  virtual void OnResponseStarted(net::URLRequest* request) {
    // It doesn't make sense for the request to have IO pending at this point.
    DCHECK(!request->status().is_io_pending());

    response_started_count_++;
    if (cancel_in_rs_) {
      request->Cancel();
      OnResponseCompleted(request);
    } else if (!request->status().is_success()) {
      DCHECK(request->status().status() == net::URLRequestStatus::FAILED ||
             request->status().status() == net::URLRequestStatus::CANCELED);
      request_failed_ = true;
      OnResponseCompleted(request);
    } else {
      // Initiate the first read.
      int bytes_read = 0;
      if (request->Read(buf_, kBufferSize, &bytes_read))
        OnReadCompleted(request, bytes_read);
      else if (!request->status().is_io_pending())
        OnResponseCompleted(request);
    }
  }

  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read) {
    // It doesn't make sense for the request to have IO pending at this point.
    DCHECK(!request->status().is_io_pending());

    if (response_started_count_ == 0)
      received_data_before_response_ = true;

    if (cancel_in_rd_)
      request->Cancel();

    if (bytes_read >= 0) {
      // There is data to read.
      received_bytes_count_ += bytes_read;

      // consume the data
      data_received_.append(buf_->data(), bytes_read);
    }

    // If it was not end of stream, request to read more.
    if (request->status().is_success() && bytes_read > 0) {
      bytes_read = 0;
      while (request->Read(buf_, kBufferSize, &bytes_read)) {
        if (bytes_read > 0) {
          data_received_.append(buf_->data(), bytes_read);
          received_bytes_count_ += bytes_read;
        } else {
          break;
        }
      }
    }
    if (!request->status().is_io_pending())
      OnResponseCompleted(request);
    else if (cancel_in_rd_pending_)
      request->Cancel();
  }

  virtual void OnResponseCompleted(net::URLRequest* request) {
    if (quit_on_complete_)
      MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* auth_info) {
    if (!username_.empty() || !password_.empty()) {
      request->SetAuth(username_, password_);
    } else {
      request->CancelAuth();
    }
  }

  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     int cert_error,
                                     net::X509Certificate* cert) {
    // The caller can control whether it needs all SSL requests to go through,
    // independent of any possible errors, or whether it wants SSL errors to
    // cancel the request.
    have_certificate_errors_ = true;
    if (allow_certificate_errors_)
      request->ContinueDespiteLastError();
    else
      request->Cancel();
  }

  virtual void OnGetCookies(net::URLRequest* request, bool blocked_by_policy) {
    if (blocked_by_policy) {
      blocked_get_cookies_count_++;
      if (cancel_in_getcookiesblocked_)
        request->Cancel();
    }
  }

  virtual void OnSetCookie(net::URLRequest* request,
                           const std::string& cookie_line,
                           const net::CookieOptions& options,
                           bool blocked_by_policy) {
    if (blocked_by_policy) {
      blocked_set_cookie_count_++;
      if (cancel_in_setcookieblocked_)
        request->Cancel();
    } else {
      set_cookie_count_++;
    }
  }

  void set_cancel_in_received_redirect(bool val) { cancel_in_rr_ = val; }
  void set_cancel_in_response_started(bool val) { cancel_in_rs_ = val; }
  void set_cancel_in_received_data(bool val) { cancel_in_rd_ = val; }
  void set_cancel_in_received_data_pending(bool val) {
    cancel_in_rd_pending_ = val;
  }
  void set_cancel_in_get_cookies_blocked(bool val) {
    cancel_in_getcookiesblocked_ = val;
  }
  void set_cancel_in_set_cookie_blocked(bool val) {
    cancel_in_setcookieblocked_ = val;
  }
  void set_quit_on_complete(bool val) { quit_on_complete_ = val; }
  void set_quit_on_redirect(bool val) { quit_on_redirect_ = val; }
  void set_allow_certificate_errors(bool val) {
    allow_certificate_errors_ = val;
  }
  void set_username(const string16& u) { username_ = u; }
  void set_password(const string16& p) { password_ = p; }

  // query state
  const std::string& data_received() const { return data_received_; }
  int bytes_received() const { return static_cast<int>(data_received_.size()); }
  int response_started_count() const { return response_started_count_; }
  int received_redirect_count() const { return received_redirect_count_; }
  int blocked_get_cookies_count() const { return blocked_get_cookies_count_; }
  int blocked_set_cookie_count() const { return blocked_set_cookie_count_; }
  int set_cookie_count() const { return set_cookie_count_; }
  bool received_data_before_response() const {
    return received_data_before_response_;
  }
  bool request_failed() const { return request_failed_; }
  bool have_certificate_errors() const { return have_certificate_errors_; }

 private:
  static const int kBufferSize = 4096;
  // options for controlling behavior
  bool cancel_in_rr_;
  bool cancel_in_rs_;
  bool cancel_in_rd_;
  bool cancel_in_rd_pending_;
  bool cancel_in_getcookiesblocked_;
  bool cancel_in_setcookieblocked_;
  bool quit_on_complete_;
  bool quit_on_redirect_;
  bool allow_certificate_errors_;

  string16 username_;
  string16 password_;

  // tracks status of callbacks
  int response_started_count_;
  int received_bytes_count_;
  int received_redirect_count_;
  int blocked_get_cookies_count_;
  int blocked_set_cookie_count_;
  int set_cookie_count_;
  bool received_data_before_response_;
  bool request_failed_;
  bool have_certificate_errors_;
  std::string data_received_;

  // our read buffer
  scoped_refptr<net::IOBuffer> buf_;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_UNITTEST_H_
