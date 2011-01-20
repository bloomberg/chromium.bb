// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HTTPS_PROBER_H_
#define NET_BASE_HTTPS_PROBER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/task.h"
#include "net/url_request/url_request.h"

template <typename T> struct DefaultSingletonTraits;

namespace net {

class URLRequestContext;

// This should be scoped inside HTTPSProber, but VC cannot compile
// HTTPProber::Delegate when HTTPSProber also inherits from
// net::URLRequest::Delegate.
class HTTPSProberDelegate {
 public:
  virtual void ProbeComplete(bool result) = 0;
 protected:
  virtual ~HTTPSProberDelegate() {}
};

// HTTPSProber is a singleton object that manages HTTPS probes. A HTTPS probe
// determines if we can connect to a given host over HTTPS. It's used when
// transparently upgrading from HTTP to HTTPS (for example, for SPDY).
class HTTPSProber : public net::URLRequest::Delegate {
 public:
  // Returns the singleton instance.
  static HTTPSProber* GetInstance();

  // HaveProbed returns true if the given host is known to have been probed
  // since the browser was last started.
  bool HaveProbed(const std::string& host) const;

  // InFlight returns true iff a probe for the given host is currently active.
  bool InFlight(const std::string& host) const;

  // ProbeHost starts a new probe for the given host. If the host is known to
  // have been probed since the browser was started, false is returned and no
  // other action is taken. If a probe to the given host in currently inflight,
  // false will be returned, and no other action is taken. Otherwise, a new
  // probe is started, true is returned and the Delegate will be called with the
  // results (true means a successful handshake).
  bool ProbeHost(const std::string& host, URLRequestContext* ctx,
                 HTTPSProberDelegate* delegate);

  // Implementation of net::URLRequest::Delegate
  virtual void OnAuthRequired(net::URLRequest* request,
                              net::AuthChallengeInfo* auth_info);
  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     int cert_error,
                                     net::X509Certificate* cert);
  virtual void OnResponseStarted(net::URLRequest* request);
  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read);

 private:
  friend struct DefaultSingletonTraits<HTTPSProber>;

  HTTPSProber();
  ~HTTPSProber();

  void Success(net::URLRequest* request);
  void Failure(net::URLRequest* request);
  void DoCallback(net::URLRequest* request, bool result);

  std::map<std::string, HTTPSProberDelegate*> inflight_probes_;
  std::set<std::string> probed_;

  DISALLOW_COPY_AND_ASSIGN(HTTPSProber);
};

}  // namespace net

#endif  // NET_BASE_HTTPS_PROBER_H_
