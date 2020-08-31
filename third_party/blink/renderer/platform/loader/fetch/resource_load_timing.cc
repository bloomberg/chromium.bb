// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"

#include "services/network/public/mojom/load_timing_info.mojom-blink.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

ResourceLoadTiming::ResourceLoadTiming() = default;

ResourceLoadTiming::ResourceLoadTiming(base::TimeTicks request_time,
                                       base::TimeTicks proxy_start,
                                       base::TimeTicks proxy_end,
                                       base::TimeTicks dns_start,
                                       base::TimeTicks dns_end,
                                       base::TimeTicks connect_start,
                                       base::TimeTicks connect_end,
                                       base::TimeTicks worker_start,
                                       base::TimeTicks worker_ready,
                                       base::TimeTicks send_start,
                                       base::TimeTicks send_end,
                                       base::TimeTicks receive_headers_start,
                                       base::TimeTicks receive_headers_end,
                                       base::TimeTicks ssl_start,
                                       base::TimeTicks ssl_end,
                                       base::TimeTicks push_start,
                                       base::TimeTicks push_end)
    : request_time_(request_time),
      proxy_start_(proxy_start),
      proxy_end_(proxy_end),
      dns_start_(dns_start),
      dns_end_(dns_end),
      connect_start_(connect_start),
      connect_end_(connect_end),
      worker_start_(worker_start),
      worker_ready_(worker_ready),
      send_start_(send_start),
      send_end_(send_end),
      receive_headers_start_(receive_headers_start),
      receive_headers_end_(receive_headers_end),
      ssl_start_(ssl_start),
      ssl_end_(ssl_end),
      push_start_(push_start),
      push_end_(push_end) {}

scoped_refptr<ResourceLoadTiming> ResourceLoadTiming::Create() {
  return base::AdoptRef(new ResourceLoadTiming);
}

scoped_refptr<ResourceLoadTiming> ResourceLoadTiming::FromMojo(
    const network::mojom::blink::LoadTimingInfo* mojo_timing) {
  if (!mojo_timing)
    return ResourceLoadTiming::Create();
  return base::AdoptRef(new ResourceLoadTiming(
      mojo_timing->request_start, mojo_timing->proxy_resolve_start,
      mojo_timing->proxy_resolve_end, mojo_timing->connect_timing->dns_start,
      mojo_timing->connect_timing->dns_end,
      mojo_timing->connect_timing->connect_start,
      mojo_timing->connect_timing->connect_end,
      mojo_timing->service_worker_start_time,
      mojo_timing->service_worker_ready_time, mojo_timing->send_start,
      mojo_timing->send_end, mojo_timing->receive_headers_start,
      mojo_timing->receive_headers_end, mojo_timing->connect_timing->ssl_start,
      mojo_timing->connect_timing->ssl_end, mojo_timing->push_start,
      mojo_timing->push_end));
}

network::mojom::blink::LoadTimingInfoPtr ResourceLoadTiming::ToMojo() const {
  network::mojom::blink::LoadTimingInfoPtr timing =
      network::mojom::blink::LoadTimingInfo::New(
          false, 0, base::Time(), request_time_, proxy_start_, proxy_end_,
          network::mojom::blink::LoadTimingInfoConnectTiming::New(
              dns_start_, dns_end_, connect_start_, connect_end_, ssl_start_,
              ssl_end_),
          send_start_, send_end_, receive_headers_start_, receive_headers_end_,
          push_start_, push_end_, worker_start_, worker_ready_);
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
         receive_headers_start_ == other.receive_headers_start_ &&
         receive_headers_end_ == other.receive_headers_end_ &&
         ssl_start_ == other.ssl_start_ && ssl_end_ == other.ssl_end_ &&
         push_start_ == other.push_start_ && push_end_ == other.push_end_;
}

bool ResourceLoadTiming::operator!=(const ResourceLoadTiming& other) const {
  return !(*this == other);
}

void ResourceLoadTiming::SetDnsStart(base::TimeTicks dns_start) {
  dns_start_ = dns_start;
}

void ResourceLoadTiming::SetRequestTime(base::TimeTicks request_time) {
  request_time_ = request_time;
}

void ResourceLoadTiming::SetProxyStart(base::TimeTicks proxy_start) {
  proxy_start_ = proxy_start;
}

void ResourceLoadTiming::SetProxyEnd(base::TimeTicks proxy_end) {
  proxy_end_ = proxy_end;
}

void ResourceLoadTiming::SetDnsEnd(base::TimeTicks dns_end) {
  dns_end_ = dns_end;
}

void ResourceLoadTiming::SetConnectStart(base::TimeTicks connect_start) {
  connect_start_ = connect_start;
}

void ResourceLoadTiming::SetConnectEnd(base::TimeTicks connect_end) {
  connect_end_ = connect_end;
}

void ResourceLoadTiming::SetWorkerStart(base::TimeTicks worker_start) {
  worker_start_ = worker_start;
}

void ResourceLoadTiming::SetWorkerReady(base::TimeTicks worker_ready) {
  worker_ready_ = worker_ready;
}

void ResourceLoadTiming::SetSendStart(base::TimeTicks send_start) {
  TRACE_EVENT_MARK_WITH_TIMESTAMP0("blink.user_timing", "requestStart",
                                   send_start);
  send_start_ = send_start;
}

void ResourceLoadTiming::SetSendEnd(base::TimeTicks send_end) {
  send_end_ = send_end;
}

void ResourceLoadTiming::SetReceiveHeadersStart(
    base::TimeTicks receive_headers_start) {
  receive_headers_start_ = receive_headers_start;
}

void ResourceLoadTiming::SetReceiveHeadersEnd(
    base::TimeTicks receive_headers_end) {
  receive_headers_end_ = receive_headers_end;
}

void ResourceLoadTiming::SetSslStart(base::TimeTicks ssl_start) {
  ssl_start_ = ssl_start;
}

void ResourceLoadTiming::SetSslEnd(base::TimeTicks ssl_end) {
  ssl_end_ = ssl_end;
}

void ResourceLoadTiming::SetPushStart(base::TimeTicks push_start) {
  push_start_ = push_start;
}

void ResourceLoadTiming::SetPushEnd(base::TimeTicks push_end) {
  push_end_ = push_end;
}

double ResourceLoadTiming::CalculateMillisecondDelta(
    base::TimeTicks time) const {
  return time.is_null() ? -1 : (time - request_time_).InMillisecondsF();
}

}  // namespace blink
