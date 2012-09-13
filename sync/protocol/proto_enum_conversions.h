// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_
#define SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_

// Keep this file in sync with the .proto files in this directory.

#include "sync/protocol/session_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

// Utility functions to get the string equivalent for some sync proto
// enums.

namespace syncer {

// The returned strings (which don't have to be freed) are in ASCII.
// The result of passing in an invalid enum value is undefined.

const char* GetBrowserTypeString(
    sync_pb::SessionWindow::BrowserType browser_type);

const char* GetPageTransitionString(
    sync_pb::SyncEnums::PageTransition page_transition);

const char* GetPageTransitionQualifierString(
    sync_pb::SyncEnums::PageTransitionQualifier
        page_transition_qualifier);

const char* GetUpdatesSourceString(
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source);

const char* GetResponseTypeString(
    sync_pb::CommitResponse::ResponseType response_type);

const char* GetErrorTypeString(sync_pb::SyncEnums::ErrorType error_type);

const char* GetActionString(sync_pb::SyncEnums::Action action);

const char* GetDeviceTypeString(
    sync_pb::SessionHeader::DeviceType device_type);

const char* GetFaviconTypeString(
    sync_pb::SessionTab::FaviconType favicon_type);

const char* PassphraseTypeString(
    sync_pb::NigoriSpecifics::PassphraseType type);

}  // namespace syncer

#endif  // SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_
