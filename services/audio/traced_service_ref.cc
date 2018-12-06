// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/traced_service_ref.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "services/service_manager/public/cpp/service_keepalive.h"

namespace audio {

TracedServiceRef::TracedServiceRef() : trace_name_("") {}

TracedServiceRef::TracedServiceRef(
    std::unique_ptr<service_manager::ServiceKeepaliveRef> keepalive_ref,
    const char* trace_name)
    : keepalive_ref_(std::move(keepalive_ref)), trace_name_(trace_name) {
  if (keepalive_ref_)
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("audio", trace_name_,
                                      keepalive_ref_.get());
}

TracedServiceRef::TracedServiceRef(TracedServiceRef&& other) {
  std::swap(keepalive_ref_, other.keepalive_ref_);
  std::swap(trace_name_, other.trace_name_);
}

TracedServiceRef& TracedServiceRef::operator=(TracedServiceRef&& other) {
  std::swap(keepalive_ref_, other.keepalive_ref_);
  std::swap(trace_name_, other.trace_name_);
  return *this;
}

TracedServiceRef::~TracedServiceRef() {
  if (keepalive_ref_)
    TRACE_EVENT_NESTABLE_ASYNC_END0("audio", trace_name_, keepalive_ref_.get());
}

}  // namespace audio
