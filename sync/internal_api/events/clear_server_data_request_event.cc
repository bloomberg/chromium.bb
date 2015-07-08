// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/events/clear_server_data_request_event.h"

#include "base/strings/stringprintf.h"
#include "sync/protocol/proto_value_conversions.h"

namespace syncer {

ClearServerDataRequestEvent::ClearServerDataRequestEvent(
    base::Time timestamp,
    const sync_pb::ClientToServerMessage& request)
    : timestamp_(timestamp), request_(request) {}

ClearServerDataRequestEvent::~ClearServerDataRequestEvent() {}

base::Time ClearServerDataRequestEvent::GetTimestamp() const {
  return timestamp_;
}

std::string ClearServerDataRequestEvent::GetType() const {
  return "ClearServerData Request";
}

std::string ClearServerDataRequestEvent::GetDetails() const {
  return std::string();
}

scoped_ptr<base::DictionaryValue> ClearServerDataRequestEvent::GetProtoMessage()
    const {
  return scoped_ptr<base::DictionaryValue>(
      ClientToServerMessageToValue(request_, false));
}

scoped_ptr<ProtocolEvent> ClearServerDataRequestEvent::Clone() const {
  return scoped_ptr<ProtocolEvent>(
      new ClearServerDataRequestEvent(timestamp_, request_));
}

}  // namespace syncer
