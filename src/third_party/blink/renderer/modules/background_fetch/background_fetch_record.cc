// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_record.h"

#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"

namespace blink {

BackgroundFetchRecord::BackgroundFetchRecord(Request* request,
                                             Response* response)
    : request_(request), response_(response) {
  DCHECK(request_);
}

BackgroundFetchRecord::~BackgroundFetchRecord() = default;

ScriptPromise BackgroundFetchRecord::responseReady(ScriptState* script_state) {
  if (!response_ready_property_) {
    response_ready_property_ =
        new ResponseReadyProperty(ExecutionContext::From(script_state), this,
                                  ResponseReadyProperty::kResponseReady);
  }
  if (!response_) {
    response_ = Response::Create(ExecutionContext::From(script_state),
                                 nullptr /* FetchResponseData */);
  }
  DCHECK(response_);
  response_ready_property_->Resolve(response_);
  return response_ready_property_->Promise(script_state->World());
}

Request* BackgroundFetchRecord::request() const {
  return request_;
}

void BackgroundFetchRecord::Trace(blink::Visitor* visitor) {
  visitor->Trace(request_);
  visitor->Trace(response_);
  visitor->Trace(response_ready_property_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
