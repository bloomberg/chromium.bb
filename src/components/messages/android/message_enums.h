// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGE_ENUMS_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGE_ENUMS_H_

namespace messages {

// List of constants describing the reasons why the message was dismissed.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.messages
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(crbug.com/1188983): Revisit enum values. TAB_SWITCHED is not currently
// used. Likely the same for TAB_DESTROYED and ACTIVITY_DESTROYED. We also need
// a dedicated value for message dismissed from feature code.
enum class DismissReason {
  // Dismiss reasons that are fully controlled by clients (i.e. are not used
  // inside the Messages implementation are marked "Controlled by client" on
  // comments.

  UNKNOWN = 0,
  // A message was dismissed by clicking on the primary action button.
  PRIMARY_ACTION = 1,
  // Controlled by client: A message was dismissed by clicking on the secondary
  // action button.
  SECONDARY_ACTION = 2,
  // A message was automatically dismissed based on timer.
  TIMER = 3,
  // A message was dismissed by user gesture.
  GESTURE = 4,
  // Controlled by client: A message was dismissed in response to switching
  // tabs.
  TAB_SWITCHED = 5,
  // Controlled by client: A message was dismissed as a result of closing a tab.
  TAB_DESTROYED = 6,
  // Controlled by client: A message is dismissed because the activity is
  // destroyed.
  ACTIVITY_DESTROYED = 7,
  // A message was dismissed due to the destruction of the corresponding scopes.
  SCOPE_DESTROYED = 8,
  // A message was dismissed explicitly in feature code.
  DISMISSED_BY_FEATURE = 9,

  // Insert new values before this line.
  COUNT
};

// "Urgent" means the user should take actions ASAP, such as responding to
// permissions or safety warnings.
enum class MessagePriority { kUrgent, kNormal };

// The constants of message scope type.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.messages
enum class MessageScopeType { WINDOW = 0, WEB_CONTENTS = 1, NAVIGATION = 2 };

// Enumerates unique identifiers for various messages. Used for recording
// messages related histograms.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// When adding a new message identifier, make corresponding changes in the
// following locations:
// - tools/metrics/histograms/enums.xml: <enum name="MessageIdentifier">
// - tools/metrics/histograms/metadata/android/histograms.xml:
//       <variants name="MessageIdentifiers">
// - MessagesMetrics.java: #messageIdentifierToHistogramSuffix()
//
// A Java counterpart is generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.messages
enum class MessageIdentifier {
  INVALID_MESSAGE = 0,
  SAVE_PASSWORD = 1,
  UPDATE_PASSWORD = 2,
  GENERATED_PASSWORD_SAVED = 3,
  POPUP_BLOCKED = 4,
  SAFETY_TIP = 5,
  SAVE_ADDRESS_PROFILE = 6,
  MERCHANT_TRUST = 7,
  ADD_TO_HOMESCREEN_IPH = 8,
  SEND_TAB_TO_SELF = 9,
  READER_MODE = 10,
  CHROME_SURVEY = 11,
  SAVE_CARD = 12,
  NOTIFICATION_BLOCKED = 13,
  PERMISSION_UPDATE = 14,
  ADS_BLOCKED = 15,
  DOWNLOAD_PROGRESS = 16,
  SYNC_ERROR = 17,
  SHARED_HIGHLIGHTING = 18,
  NEAR_OOM_REDUCTION = 19,
  INSTALLABLE_AMBIENT_BADGE = 20,
  AUTO_DARK_WEB_CONTENTS = 21,
  TEST_MESSAGE = 22,
  TAILORED_SECURITY_ENABLED = 23,
  VR_SERVICES_UPGRADE = 24,
  TAILORED_SECURITY_DISABLED = 25,
  AR_CORE_UPGRADE = 26,

  // Insert new values before this line.
  COUNT
};

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGE_ENUMS_H_
