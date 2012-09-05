// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#include "sync/protocol/proto_enum_conversions.h"

#include "base/basictypes.h"
#include "base/logging.h"

namespace syncer {

#define ASSERT_ENUM_BOUNDS(enum_parent, enum_type, enum_min, enum_max)  \
  COMPILE_ASSERT(enum_parent::enum_type##_MIN == enum_parent::enum_min, \
                 enum_type##_MIN_not_##enum_min);                       \
  COMPILE_ASSERT(enum_parent::enum_type##_MAX == enum_parent::enum_max, \
                 enum_type##_MAX_not_##enum_max);

#define ENUM_CASE(enum_parent, enum_value)              \
  case enum_parent::enum_value: return #enum_value

const char* GetBrowserTypeString(
    sync_pb::SessionWindow::BrowserType browser_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SessionWindow, BrowserType,
                     TYPE_TABBED, TYPE_POPUP);
  switch (browser_type) {
    ENUM_CASE(sync_pb::SessionWindow, TYPE_TABBED);
    ENUM_CASE(sync_pb::SessionWindow, TYPE_POPUP);
  }
  NOTREACHED();
  return "";
}

const char* GetPageTransitionString(
    sync_pb::SyncEnums::PageTransition page_transition) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, PageTransition,
                     LINK, CHAIN_END);
  switch (page_transition) {
    ENUM_CASE(sync_pb::SyncEnums, LINK);
    ENUM_CASE(sync_pb::SyncEnums, TYPED);
    ENUM_CASE(sync_pb::SyncEnums, AUTO_BOOKMARK);
    ENUM_CASE(sync_pb::SyncEnums, AUTO_SUBFRAME);
    ENUM_CASE(sync_pb::SyncEnums, MANUAL_SUBFRAME);
    ENUM_CASE(sync_pb::SyncEnums, GENERATED);
    ENUM_CASE(sync_pb::SyncEnums, AUTO_TOPLEVEL);
    ENUM_CASE(sync_pb::SyncEnums, FORM_SUBMIT);
    ENUM_CASE(sync_pb::SyncEnums, RELOAD);
    ENUM_CASE(sync_pb::SyncEnums, KEYWORD);
    ENUM_CASE(sync_pb::SyncEnums, KEYWORD_GENERATED);
    ENUM_CASE(sync_pb::SyncEnums, CHAIN_START);
    ENUM_CASE(sync_pb::SyncEnums, CHAIN_END);
  }
  NOTREACHED();
  return "";
}

const char* GetPageTransitionQualifierString(
    sync_pb::SyncEnums::PageTransitionQualifier
        page_transition_qualifier) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, PageTransitionQualifier,
                     CLIENT_REDIRECT, SERVER_REDIRECT);
  switch (page_transition_qualifier) {
    ENUM_CASE(sync_pb::SyncEnums, CLIENT_REDIRECT);
    ENUM_CASE(sync_pb::SyncEnums, SERVER_REDIRECT);
  }
  NOTREACHED();
  return "";
}

const char* GetUpdatesSourceString(
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source) {
  ASSERT_ENUM_BOUNDS(sync_pb::GetUpdatesCallerInfo, GetUpdatesSource,
                     UNKNOWN, DATATYPE_REFRESH);
  switch (updates_source) {
    ENUM_CASE(sync_pb::GetUpdatesCallerInfo, UNKNOWN);
    ENUM_CASE(sync_pb::GetUpdatesCallerInfo, FIRST_UPDATE);
    ENUM_CASE(sync_pb::GetUpdatesCallerInfo, LOCAL);
    ENUM_CASE(sync_pb::GetUpdatesCallerInfo, NOTIFICATION);
    ENUM_CASE(sync_pb::GetUpdatesCallerInfo, PERIODIC);
    ENUM_CASE(sync_pb::GetUpdatesCallerInfo, SYNC_CYCLE_CONTINUATION);
    ENUM_CASE(sync_pb::GetUpdatesCallerInfo, NEWLY_SUPPORTED_DATATYPE);
    ENUM_CASE(sync_pb::GetUpdatesCallerInfo, MIGRATION);
    ENUM_CASE(sync_pb::GetUpdatesCallerInfo, NEW_CLIENT);
    ENUM_CASE(sync_pb::GetUpdatesCallerInfo, RECONFIGURATION);
    ENUM_CASE(sync_pb::GetUpdatesCallerInfo, DATATYPE_REFRESH);
  }
  NOTREACHED();
  return "";
}

const char* GetResponseTypeString(
    sync_pb::CommitResponse::ResponseType response_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::CommitResponse, ResponseType, SUCCESS,
                     TRANSIENT_ERROR);
  switch (response_type) {
    ENUM_CASE(sync_pb::CommitResponse, SUCCESS);
    ENUM_CASE(sync_pb::CommitResponse, CONFLICT);
    ENUM_CASE(sync_pb::CommitResponse, RETRY);
    ENUM_CASE(sync_pb::CommitResponse, INVALID_MESSAGE);
    ENUM_CASE(sync_pb::CommitResponse, OVER_QUOTA);
    ENUM_CASE(sync_pb::CommitResponse, TRANSIENT_ERROR);
  }
  NOTREACHED();
  return "";
}

const char* GetErrorTypeString(sync_pb::SyncEnums::ErrorType error_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, ErrorType, SUCCESS, UNKNOWN);
  switch (error_type) {
    ENUM_CASE(sync_pb::SyncEnums, SUCCESS);
    ENUM_CASE(sync_pb::SyncEnums, ACCESS_DENIED);
    ENUM_CASE(sync_pb::SyncEnums, NOT_MY_BIRTHDAY);
    ENUM_CASE(sync_pb::SyncEnums, THROTTLED);
    ENUM_CASE(sync_pb::SyncEnums, AUTH_EXPIRED);
    ENUM_CASE(sync_pb::SyncEnums, USER_NOT_ACTIVATED);
    ENUM_CASE(sync_pb::SyncEnums, AUTH_INVALID);
    ENUM_CASE(sync_pb::SyncEnums, CLEAR_PENDING);
    ENUM_CASE(sync_pb::SyncEnums, TRANSIENT_ERROR);
    ENUM_CASE(sync_pb::SyncEnums, MIGRATION_DONE);
    ENUM_CASE(sync_pb::SyncEnums, UNKNOWN);
  }
  NOTREACHED();
  return "";
}

const char* GetActionString(sync_pb::SyncEnums::Action action) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, Action,
                     UPGRADE_CLIENT, UNKNOWN_ACTION);
  switch (action) {
    ENUM_CASE(sync_pb::SyncEnums, UPGRADE_CLIENT);
    ENUM_CASE(sync_pb::SyncEnums, CLEAR_USER_DATA_AND_RESYNC);
    ENUM_CASE(sync_pb::SyncEnums, ENABLE_SYNC_ON_ACCOUNT);
    ENUM_CASE(sync_pb::SyncEnums, STOP_AND_RESTART_SYNC);
    ENUM_CASE(sync_pb::SyncEnums, DISABLE_SYNC_ON_CLIENT);
    ENUM_CASE(sync_pb::SyncEnums, UNKNOWN_ACTION);
  }
  NOTREACHED();
  return "";

}

const char* GetDeviceTypeString(
    sync_pb::SessionHeader::DeviceType device_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SessionHeader, DeviceType, TYPE_WIN, TYPE_TABLET);
  switch (device_type) {
    ENUM_CASE(sync_pb::SessionHeader, TYPE_WIN);
    ENUM_CASE(sync_pb::SessionHeader, TYPE_MAC);
    ENUM_CASE(sync_pb::SessionHeader, TYPE_LINUX);
    ENUM_CASE(sync_pb::SessionHeader, TYPE_CROS);
    ENUM_CASE(sync_pb::SessionHeader, TYPE_OTHER);
    ENUM_CASE(sync_pb::SessionHeader, TYPE_PHONE);
    ENUM_CASE(sync_pb::SessionHeader, TYPE_TABLET);
  }
  NOTREACHED();
  return "";
}

const char* GetFaviconTypeString(
    sync_pb::SessionTab::FaviconType favicon_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SessionTab, FaviconType, TYPE_WEB_FAVICON,
                     TYPE_WEB_FAVICON);
  switch (favicon_type) {
    ENUM_CASE(sync_pb::SessionTab, TYPE_WEB_FAVICON);
  }
  NOTREACHED();
  return "";
}

#undef ASSERT_ENUM_BOUNDS
#undef ENUM_CASE

}  // namespace syncer
