/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceLoadTiming_h
#define ResourceLoadTiming_h

#include "platform/PlatformExport.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class PLATFORM_EXPORT ResourceLoadTiming
    : public RefCounted<ResourceLoadTiming> {
 public:
  static RefPtr<ResourceLoadTiming> Create();

  RefPtr<ResourceLoadTiming> DeepCopy();
  bool operator==(const ResourceLoadTiming&) const;
  bool operator!=(const ResourceLoadTiming&) const;

  void SetDnsStart(double);
  void SetRequestTime(double);
  void SetProxyStart(double);
  void SetProxyEnd(double);
  void SetDnsEnd(double);
  void SetConnectStart(double);
  void SetConnectEnd(double);
  void SetWorkerStart(double);
  void SetWorkerReady(double);
  void SetSendStart(double);
  void SetSendEnd(double);
  void SetReceiveHeadersEnd(double);
  void SetSslStart(double);
  void SetSslEnd(double);
  void SetPushStart(double);
  void SetPushEnd(double);

  double DnsStart() const { return dns_start_; }
  double RequestTime() const { return request_time_; }
  double ProxyStart() const { return proxy_start_; }
  double ProxyEnd() const { return proxy_end_; }
  double DnsEnd() const { return dns_end_; }
  double ConnectStart() const { return connect_start_; }
  double ConnectEnd() const { return connect_end_; }
  double WorkerStart() const { return worker_start_; }
  double WorkerReady() const { return worker_ready_; }
  double SendStart() const { return send_start_; }
  double SendEnd() const { return send_end_; }
  double ReceiveHeadersEnd() const { return receive_headers_end_; }
  double SslStart() const { return ssl_start_; }
  double SslEnd() const { return ssl_end_; }
  double PushStart() const { return push_start_; }
  double PushEnd() const { return push_end_; }

  double CalculateMillisecondDelta(double) const;

 private:
  ResourceLoadTiming();

  // We want to present a unified timeline to Javascript. Using walltime is
  // problematic, because the clock may skew while resources load. To prevent
  // that skew, we record a single reference walltime when root document
  // navigation begins. All other times are recorded using
  // monotonicallyIncreasingTime(). When a time needs to be presented to
  // Javascript, we build a pseudo-walltime using the following equation
  // (m_requestTime as example):
  //   pseudo time = document wall reference +
  //                     (m_requestTime - document monotonic reference).

  // All monotonicallyIncreasingTime() in seconds
  double request_time_;
  double proxy_start_;
  double proxy_end_;
  double dns_start_;
  double dns_end_;
  double connect_start_;
  double connect_end_;
  double worker_start_;
  double worker_ready_;
  double send_start_;
  double send_end_;
  double receive_headers_end_;
  double ssl_start_;
  double ssl_end_;
  double push_start_;
  double push_end_;
};

}  // namespace blink

#endif
