// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

const char kTemporary[] = "temporary";
const char kPersistentLicense[] = "persistent-license";
const char kPersistentUsageRecord[] = "persistent-usage-record";

}  // namespace

// static
WebEncryptedMediaInitDataType EncryptedMediaUtils::ConvertToInitDataType(
    const String& init_data_type) {
  if (init_data_type == "cenc")
    return WebEncryptedMediaInitDataType::kCenc;
  if (init_data_type == "keyids")
    return WebEncryptedMediaInitDataType::kKeyids;
  if (init_data_type == "webm")
    return WebEncryptedMediaInitDataType::kWebm;

  // |initDataType| is not restricted in the idl, so anything is possible.
  return WebEncryptedMediaInitDataType::kUnknown;
}

// static
String EncryptedMediaUtils::ConvertFromInitDataType(
    WebEncryptedMediaInitDataType init_data_type) {
  switch (init_data_type) {
    case WebEncryptedMediaInitDataType::kCenc:
      return "cenc";
    case WebEncryptedMediaInitDataType::kKeyids:
      return "keyids";
    case WebEncryptedMediaInitDataType::kWebm:
      return "webm";
    case WebEncryptedMediaInitDataType::kUnknown:
      // Chromium should not use Unknown, but we use it in Blink when the
      // actual value has been blocked for non-same-origin or mixed content.
      return String();
  }

  NOTREACHED();
  return String();
}

// static
WebEncryptedMediaSessionType EncryptedMediaUtils::ConvertToSessionType(
    const String& session_type) {
  if (session_type == kTemporary)
    return WebEncryptedMediaSessionType::kTemporary;
  if (session_type == kPersistentLicense)
    return WebEncryptedMediaSessionType::kPersistentLicense;
  if (session_type == kPersistentUsageRecord &&
      RuntimeEnabledFeatures::
          EncryptedMediaPersistentUsageRecordSessionEnabled()) {
    return WebEncryptedMediaSessionType::kPersistentUsageRecord;
  }

  // |sessionType| is not restricted in the idl, so anything is possible.
  return WebEncryptedMediaSessionType::kUnknown;
}

// static
String EncryptedMediaUtils::ConvertFromSessionType(
    WebEncryptedMediaSessionType session_type) {
  switch (session_type) {
    case WebEncryptedMediaSessionType::kTemporary:
      return kTemporary;
    case WebEncryptedMediaSessionType::kPersistentLicense:
      return kPersistentLicense;
    case WebEncryptedMediaSessionType::kPersistentUsageRecord:
      if (RuntimeEnabledFeatures::
              EncryptedMediaPersistentUsageRecordSessionEnabled()) {
        return kPersistentUsageRecord;
      }
      FALLTHROUGH;
    case WebEncryptedMediaSessionType::kUnknown:
      // Unexpected session type from Chromium.
      NOTREACHED();
      return String();
  }

  NOTREACHED();
  return String();
}

// static
String EncryptedMediaUtils::ConvertKeyStatusToString(
    const WebEncryptedMediaKeyInformation::KeyStatus status) {
  switch (status) {
    case WebEncryptedMediaKeyInformation::KeyStatus::kUsable:
      return "usable";
    case WebEncryptedMediaKeyInformation::KeyStatus::kExpired:
      return "expired";
    case WebEncryptedMediaKeyInformation::KeyStatus::kReleased:
      return "released";
    case WebEncryptedMediaKeyInformation::KeyStatus::kOutputRestricted:
      return "output-restricted";
    case WebEncryptedMediaKeyInformation::KeyStatus::kOutputDownscaled:
      return "output-downscaled";
    case WebEncryptedMediaKeyInformation::KeyStatus::kStatusPending:
      return "status-pending";
    case WebEncryptedMediaKeyInformation::KeyStatus::kInternalError:
      return "internal-error";
  }

  NOTREACHED();
  return "internal-error";
}

// static
WebMediaKeySystemConfiguration::Requirement
EncryptedMediaUtils::ConvertToMediaKeysRequirement(const String& requirement) {
  if (requirement == "required")
    return WebMediaKeySystemConfiguration::Requirement::kRequired;
  if (requirement == "optional")
    return WebMediaKeySystemConfiguration::Requirement::kOptional;
  if (requirement == "not-allowed")
    return WebMediaKeySystemConfiguration::Requirement::kNotAllowed;

  NOTREACHED();
  return WebMediaKeySystemConfiguration::Requirement::kOptional;
}

// static
String EncryptedMediaUtils::ConvertMediaKeysRequirementToString(
    WebMediaKeySystemConfiguration::Requirement requirement) {
  switch (requirement) {
    case WebMediaKeySystemConfiguration::Requirement::kRequired:
      return "required";
    case WebMediaKeySystemConfiguration::Requirement::kOptional:
      return "optional";
    case WebMediaKeySystemConfiguration::Requirement::kNotAllowed:
      return "not-allowed";
  }

  NOTREACHED();
  return "not-allowed";
}

}  // namespace blink
