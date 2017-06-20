// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/ResourceLoadTiming.h"

#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

ResourceLoadTiming::ResourceLoadTiming()
    : request_time_(0),
      proxy_start_(0),
      proxy_end_(0),
      dns_start_(0),
      dns_end_(0),
      connect_start_(0),
      connect_end_(0),
      worker_start_(0),
      worker_ready_(0),
      send_start_(0),
      send_end_(0),
      receive_headers_end_(0),
      ssl_start_(0),
      ssl_end_(0),
      push_start_(0),
      push_end_(0) {}

RefPtr<ResourceLoadTiming> ResourceLoadTiming::Create() {
  return AdoptRef(new ResourceLoadTiming);
}

RefPtr<ResourceLoadTiming> ResourceLoadTiming::DeepCopy() {
  RefPtr<ResourceLoadTiming> timing = Create();
  timing->request_time_ = request_time_;
  timing->proxy_start_ = proxy_start_;
  timing->proxy_end_ = proxy_end_;
  timing->dns_start_ = dns_start_;
  timing->dns_end_ = dns_end_;
  timing->connect_start_ = connect_start_;
  timing->connect_end_ = connect_end_;
  timing->worker_start_ = worker_start_;
  timing->worker_ready_ = worker_ready_;
  timing->send_start_ = send_start_;
  timing->send_end_ = send_end_;
  timing->receive_headers_end_ = receive_headers_end_;
  timing->ssl_start_ = ssl_start_;
  timing->ssl_end_ = ssl_end_;
  timing->push_start_ = push_start_;
  timing->push_end_ = push_end_;
  return timing;
}

bool ResourceLoadTiming::operator==(const ResourceLoadTiming& other) const {
  return request_time_ == other.request_time_ &&
         proxy_start_ == other.proxy_start_ && proxy_end_ == other.proxy_end_ &&
         dns_start_ == other.dns_start_ && dns_end_ == other.dns_end_ &&
         connect_start_ == other.connect_start_ &&
         connect_end_ == other.connect_end_ &&
         worker_start_ == other.worker_start_ &&
         worker_ready_ == other.worker_ready_ &&
         send_start_ == other.send_start_ && send_end_ == other.send_end_ &&
         receive_headers_end_ == other.receive_headers_end_ &&
         ssl_start_ == other.ssl_start_ && ssl_end_ == other.ssl_end_ &&
         push_start_ == other.push_start_ && push_end_ == other.push_end_;
}

bool ResourceLoadTiming::operator!=(const ResourceLoadTiming& other) const {
  return !(*this == other);
}

void ResourceLoadTiming::SetDnsStart(double dns_start) {
  dns_start_ = dns_start;
}

void ResourceLoadTiming::SetRequestTime(double request_time) {
  request_time_ = request_time;
}

void ResourceLoadTiming::SetProxyStart(double proxy_start) {
  proxy_start_ = proxy_start;
}

void ResourceLoadTiming::SetProxyEnd(double proxy_end) {
  proxy_end_ = proxy_end;
}

void ResourceLoadTiming::SetDnsEnd(double dns_end) {
  dns_end_ = dns_end;
}

void ResourceLoadTiming::SetConnectStart(double connect_start) {
  connect_start_ = connect_start;
}

void ResourceLoadTiming::SetConnectEnd(double connect_end) {
  connect_end_ = connect_end;
}

void ResourceLoadTiming::SetWorkerStart(double worker_start) {
  worker_start_ = worker_start;
}

void ResourceLoadTiming::SetWorkerReady(double worker_ready) {
  worker_ready_ = worker_ready;
}

void ResourceLoadTiming::SetSendStart(double send_start) {
  TRACE_EVENT_MARK_WITH_TIMESTAMP0("blink.user_timing", "requestStart",
                                   TraceEvent::ToTraceTimestamp(send_start));
  send_start_ = send_start;
}

void ResourceLoadTiming::SetSendEnd(double send_end) {
  send_end_ = send_end;
}

void ResourceLoadTiming::SetReceiveHeadersEnd(double receive_headers_end) {
  receive_headers_end_ = receive_headers_end;
}

void ResourceLoadTiming::SetSslStart(double ssl_start) {
  ssl_start_ = ssl_start;
}

void ResourceLoadTiming::SetSslEnd(double ssl_end) {
  ssl_end_ = ssl_end;
}

void ResourceLoadTiming::SetPushStart(double push_start) {
  push_start_ = push_start;
}

void ResourceLoadTiming::SetPushEnd(double push_end) {
  push_end_ = push_end;
}

double ResourceLoadTiming::CalculateMillisecondDelta(double time) const {
  return time ? (time - request_time_) * 1000 : -1;
}

}  // namespace blink
