/*
 * Copyright (C) 2013 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_TIMING_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_TIMING_INFO_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/public/mojom/timing/worker_timing_container.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class PLATFORM_EXPORT ResourceTimingInfo
    : public RefCounted<ResourceTimingInfo> {
  USING_FAST_MALLOC(ResourceTimingInfo);

 public:
  static scoped_refptr<ResourceTimingInfo> Create(
      const AtomicString& type,
      const base::TimeTicks time,
      mojom::RequestContextType context,
      network::mojom::RequestDestination destination) {
    return base::AdoptRef(
        new ResourceTimingInfo(type, time, context, destination));
  }
  base::TimeTicks InitialTime() const { return initial_time_; }

  const AtomicString& InitiatorType() const { return type_; }

  void SetLoadResponseEnd(base::TimeTicks time) { load_response_end_ = time; }
  base::TimeTicks LoadResponseEnd() const { return load_response_end_; }

  void SetInitialURL(const KURL& url) { initial_url_ = url; }
  const KURL& InitialURL() const { return initial_url_; }

  void SetFinalResponse(const ResourceResponse& response) {
    final_response_ = response;
  }
  const ResourceResponse& FinalResponse() const { return final_response_; }

  void AddRedirect(const ResourceResponse& redirect_response,
                   const KURL& new_url);
  const Vector<ResourceResponse>& RedirectChain() const {
    return redirect_chain_;
  }

  void AddFinalTransferSize(uint64_t encoded_data_length) {
    transfer_size_ += encoded_data_length;
  }
  uint64_t TransferSize() const { return transfer_size_; }

  // The timestamps in PerformanceResourceTiming are measured relative from the
  // time origin. In most cases these timestamps must be positive value, so we
  // use 0 for invalid negative values. But the timestamps for Service Worker
  // navigation preload requests may be negative, because these requests may
  // be started before the service worker started. We set this flag true, to
  // support such case.
  void SetNegativeAllowed(bool negative_allowed) {
    negative_allowed_ = negative_allowed;
  }
  bool NegativeAllowed() const { return negative_allowed_; }
  mojom::RequestContextType ContextType() const { return context_type_; }
  network::mojom::RequestDestination RequestDestination() const {
    return request_destination_;
  }

  void SetWorkerTimingReceiver(
      mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
          worker_timing_receiver) {
    worker_timing_receiver_ = std::move(worker_timing_receiver);
  }

  mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
  TakeWorkerTimingReceiver() const {
    return std::move(worker_timing_receiver_);
  }

 private:
  ResourceTimingInfo(const AtomicString& type,
                     const base::TimeTicks time,
                     mojom::RequestContextType context_type,
                     network::mojom::RequestDestination request_destination)
      : type_(type),
        initial_time_(time),
        context_type_(context_type),
        request_destination_(request_destination) {}

  AtomicString type_;
  base::TimeTicks initial_time_;
  mojom::RequestContextType context_type_;
  network::mojom::RequestDestination request_destination_;
  base::TimeTicks load_response_end_;
  KURL initial_url_;
  ResourceResponse final_response_;
  Vector<ResourceResponse> redirect_chain_;
  uint64_t transfer_size_ = 0;
  bool has_cross_origin_redirect_ = false;
  bool negative_allowed_ = false;

  // Mutable since it must be passed to blink::PerformanceResourceTiming by move
  // semantics but ResourceTimingInfo is passed only by const reference.
  // ResourceTimingInfo can't be changed to pass by value because it can
  // actually be a large object.
  // It can be null when service worker doesn't serve a response for the
  // resource. In that case, PerformanceResourceTiming#workerTiming is kept
  // empty.
  mutable mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
      worker_timing_receiver_;

  DISALLOW_COPY_AND_ASSIGN(ResourceTimingInfo);
};

}  // namespace blink

#endif
