// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_record.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

BackgroundFetchRecord::BackgroundFetchRecord(Request* request,
                                             ScriptState* script_state)
    : request_(request), script_state_(script_state) {
  DCHECK(request_);
  DCHECK(script_state_);
  response_ready_property_ =
      new ResponseReadyProperty(ExecutionContext::From(script_state), this,
                                ResponseReadyProperty::kResponseReady);
}

BackgroundFetchRecord::~BackgroundFetchRecord() = default;

void BackgroundFetchRecord::ResolveResponseReadyProperty() {
  if (!response_ready_property_ ||
      response_ready_property_->GetState() !=
          ScriptPromisePropertyBase::State::kPending) {
    return;
  }

  switch (record_state_) {
    case State::kPending:
      return;
    case State::kAborted:
      response_ready_property_->Reject(DOMException::Create(
          DOMExceptionCode::kAbortError,
          "The fetch was aborted before the record was processed."));
      return;
    case State::kSettled:
      if (response_) {
        response_ready_property_->Resolve(response_);
        return;
      }

      if (!script_state_->ContextIsValid())
        return;

      // TODO(crbug.com/875201):Per https://wicg.github.io/background-fetch/
      // #background-fetch-response-exposed, this should be resolved with a
      // TypeError. Figure out a way to do so.
      // Rejecting this with a TypeError here doesn't work because the
      // RejectedType is a DOMException. Update this with the correct error
      // once confirmed, or change the RejectedType.
      response_ready_property_->Reject(DOMException::Create(
          DOMExceptionCode::kUnknownError, "The response is not available."));
  }
}

ScriptPromise BackgroundFetchRecord::responseReady(ScriptState* script_state) {
  return response_ready_property_->Promise(script_state->World());
}

Request* BackgroundFetchRecord::request() const {
  return request_;
}

void BackgroundFetchRecord::UpdateState(
    BackgroundFetchRecord::State updated_state) {
  DCHECK_EQ(record_state_, State::kPending);

  record_state_ = updated_state;
  ResolveResponseReadyProperty();
}

void BackgroundFetchRecord::SetResponse(
    mojom::blink::FetchAPIResponsePtr& response) {
  DCHECK(record_state_ == State::kPending);

  if (!response_) {
    if (!script_state_->ContextIsValid())
      return;
    response_ = Response::Create(script_state_, *response);
  }

  DCHECK(response_);

  ResolveResponseReadyProperty();
}

bool BackgroundFetchRecord::IsRecordPending() {
  return record_state_ == State::kPending;
}

void BackgroundFetchRecord::Trace(blink::Visitor* visitor) {
  visitor->Trace(request_);
  visitor->Trace(response_);
  visitor->Trace(response_ready_property_);
  visitor->Trace(script_state_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
