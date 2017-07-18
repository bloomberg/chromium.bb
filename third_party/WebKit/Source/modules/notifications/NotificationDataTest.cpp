// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationData.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/testing/NullExecutionContext.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationOptions.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

const char kNotificationBaseUrl[] = "https://example.com/directory/";
const char kNotificationTitle[] = "My Notification";

const char kNotificationDir[] = "rtl";
const char kNotificationLang[] = "nl";
const char kNotificationBody[] = "Hello, world";
const char kNotificationTag[] = "my_tag";
const char kNotificationEmptyTag[] = "";
const char kNotificationImage[] = "https://example.com/image.jpg";
const char kNotificationIcon[] = "/icon.png";
const char kNotificationIconInvalid[] = "https://invalid:icon:url";
const char kNotificationBadge[] = "badge.png";
const unsigned kNotificationVibration[] = {42, 10, 20, 30, 40};
const unsigned long long kNotificationTimestamp = 621046800ull;
const bool kNotificationRenotify = true;
const bool kNotificationSilent = false;
const bool kNotificationRequireInteraction = true;

const WebNotificationAction::Type kWebNotificationActionType =
    WebNotificationAction::kText;
const char kNotificationActionType[] = "text";
const char kNotificationActionAction[] = "my_action";
const char kNotificationActionTitle[] = "My Action";
const char kNotificationActionIcon[] = "https://example.com/action_icon.png";
const char kNotificationActionPlaceholder[] = "Placeholder...";

const unsigned kNotificationVibrationUnnormalized[] = {10, 1000000, 50, 42};
const int kNotificationVibrationNormalized[] = {10, 10000, 50};

// Execution context that implements the VirtualCompleteURL method to complete
// URLs that are assumed to be relative against a given base URL.
class CompleteUrlExecutionContext final : public NullExecutionContext {
 public:
  explicit CompleteUrlExecutionContext(const String& base)
      : base_(kParsedURLString, base) {}

 protected:
  ~CompleteUrlExecutionContext() final = default;

  KURL VirtualCompleteURL(const String& url) const override {
    return KURL(base_, url);
  }

 private:
  KURL base_;
};

class NotificationDataTest : public ::testing::Test {
 public:
  void SetUp() override {
    execution_context_ = new CompleteUrlExecutionContext(kNotificationBaseUrl);
  }

  ExecutionContext* GetExecutionContext() { return execution_context_.Get(); }

 private:
  Persistent<ExecutionContext> execution_context_;
};

TEST_F(NotificationDataTest, ReflectProperties) {
  Vector<unsigned> vibration_pattern;
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(kNotificationVibration); ++i)
    vibration_pattern.push_back(kNotificationVibration[i]);

  UnsignedLongOrUnsignedLongSequence vibration_sequence;
  vibration_sequence.setUnsignedLongSequence(vibration_pattern);

  HeapVector<NotificationAction> actions;
  for (size_t i = 0; i < Notification::maxActions(); ++i) {
    NotificationAction action;
    action.setType(kNotificationActionType);
    action.setAction(kNotificationActionAction);
    action.setTitle(kNotificationActionTitle);
    action.setIcon(kNotificationActionIcon);
    action.setPlaceholder(kNotificationActionPlaceholder);

    actions.push_back(action);
  }

  NotificationOptions options;
  options.setDir(kNotificationDir);
  options.setLang(kNotificationLang);
  options.setBody(kNotificationBody);
  options.setTag(kNotificationTag);
  options.setImage(kNotificationImage);
  options.setIcon(kNotificationIcon);
  options.setBadge(kNotificationBadge);
  options.setVibrate(vibration_sequence);
  options.setTimestamp(kNotificationTimestamp);
  options.setRenotify(kNotificationRenotify);
  options.setSilent(kNotificationSilent);
  options.setRequireInteraction(kNotificationRequireInteraction);
  options.setActions(actions);

  // TODO(peter): Test |options.data| and |notificationData.data|.

  DummyExceptionStateForTesting exception_state;
  WebNotificationData notification_data = CreateWebNotificationData(
      GetExecutionContext(), kNotificationTitle, options, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  EXPECT_EQ(kNotificationTitle, notification_data.title);

  EXPECT_EQ(WebNotificationData::kDirectionRightToLeft,
            notification_data.direction);
  EXPECT_EQ(kNotificationLang, notification_data.lang);
  EXPECT_EQ(kNotificationBody, notification_data.body);
  EXPECT_EQ(kNotificationTag, notification_data.tag);

  KURL base(kParsedURLString, kNotificationBaseUrl);

  // URLs should be resolved against the base URL of the execution context.
  EXPECT_EQ(WebURL(KURL(base, kNotificationImage)), notification_data.image);
  EXPECT_EQ(WebURL(KURL(base, kNotificationIcon)), notification_data.icon);
  EXPECT_EQ(WebURL(KURL(base, kNotificationBadge)), notification_data.badge);

  ASSERT_EQ(vibration_pattern.size(), notification_data.vibrate.size());
  for (size_t i = 0; i < vibration_pattern.size(); ++i)
    EXPECT_EQ(vibration_pattern[i],
              static_cast<unsigned>(notification_data.vibrate[i]));

  EXPECT_EQ(kNotificationTimestamp, notification_data.timestamp);
  EXPECT_EQ(kNotificationRenotify, notification_data.renotify);
  EXPECT_EQ(kNotificationSilent, notification_data.silent);
  EXPECT_EQ(kNotificationRequireInteraction,
            notification_data.require_interaction);
  EXPECT_EQ(actions.size(), notification_data.actions.size());
  for (const auto& action : notification_data.actions) {
    EXPECT_EQ(kWebNotificationActionType, action.type);
    EXPECT_EQ(kNotificationActionAction, action.action);
    EXPECT_EQ(kNotificationActionTitle, action.title);
    EXPECT_EQ(kNotificationActionPlaceholder, action.placeholder);
  }
}

TEST_F(NotificationDataTest, SilentNotificationWithVibration) {
  Vector<unsigned> vibration_pattern;
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(kNotificationVibration); ++i)
    vibration_pattern.push_back(kNotificationVibration[i]);

  UnsignedLongOrUnsignedLongSequence vibration_sequence;
  vibration_sequence.setUnsignedLongSequence(vibration_pattern);

  NotificationOptions options;
  options.setVibrate(vibration_sequence);
  options.setSilent(true);

  DummyExceptionStateForTesting exception_state;
  WebNotificationData notification_data = CreateWebNotificationData(
      GetExecutionContext(), kNotificationTitle, options, exception_state);
  ASSERT_TRUE(exception_state.HadException());

  EXPECT_EQ("Silent notifications must not specify vibration patterns.",
            exception_state.Message());
}

TEST_F(NotificationDataTest, ActionTypeButtonWithPlaceholder) {
  HeapVector<NotificationAction> actions;
  NotificationAction action;
  action.setType("button");
  action.setPlaceholder("I'm afraid I can't do that...");
  actions.push_back(action);

  NotificationOptions options;
  options.setActions(actions);

  DummyExceptionStateForTesting exception_state;
  WebNotificationData notification_data = CreateWebNotificationData(
      GetExecutionContext(), kNotificationTitle, options, exception_state);
  ASSERT_TRUE(exception_state.HadException());

  EXPECT_EQ("Notifications of type \"button\" cannot specify a placeholder.",
            exception_state.Message());
}

TEST_F(NotificationDataTest, RenotifyWithEmptyTag) {
  NotificationOptions options;
  options.setTag(kNotificationEmptyTag);
  options.setRenotify(true);

  DummyExceptionStateForTesting exception_state;
  WebNotificationData notification_data = CreateWebNotificationData(
      GetExecutionContext(), kNotificationTitle, options, exception_state);
  ASSERT_TRUE(exception_state.HadException());

  EXPECT_EQ(
      "Notifications which set the renotify flag must specify a non-empty tag.",
      exception_state.Message());
}

TEST_F(NotificationDataTest, InvalidIconUrls) {
  HeapVector<NotificationAction> actions;
  for (size_t i = 0; i < Notification::maxActions(); ++i) {
    NotificationAction action;
    action.setAction(kNotificationActionAction);
    action.setTitle(kNotificationActionTitle);
    action.setIcon(kNotificationIconInvalid);
    actions.push_back(action);
  }

  NotificationOptions options;
  options.setImage(kNotificationIconInvalid);
  options.setIcon(kNotificationIconInvalid);
  options.setBadge(kNotificationIconInvalid);
  options.setActions(actions);

  DummyExceptionStateForTesting exception_state;
  WebNotificationData notification_data = CreateWebNotificationData(
      GetExecutionContext(), kNotificationTitle, options, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  EXPECT_TRUE(notification_data.image.IsEmpty());
  EXPECT_TRUE(notification_data.icon.IsEmpty());
  EXPECT_TRUE(notification_data.badge.IsEmpty());
  for (const auto& action : notification_data.actions)
    EXPECT_TRUE(action.icon.IsEmpty());
}

TEST_F(NotificationDataTest, VibrationNormalization) {
  Vector<unsigned> unnormalized_pattern;
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(kNotificationVibrationUnnormalized);
       ++i)
    unnormalized_pattern.push_back(kNotificationVibrationUnnormalized[i]);

  UnsignedLongOrUnsignedLongSequence vibration_sequence;
  vibration_sequence.setUnsignedLongSequence(unnormalized_pattern);

  NotificationOptions options;
  options.setVibrate(vibration_sequence);

  DummyExceptionStateForTesting exception_state;
  WebNotificationData notification_data = CreateWebNotificationData(
      GetExecutionContext(), kNotificationTitle, options, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  Vector<int> normalized_pattern;
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(kNotificationVibrationNormalized);
       ++i)
    normalized_pattern.push_back(kNotificationVibrationNormalized[i]);

  ASSERT_EQ(normalized_pattern.size(), notification_data.vibrate.size());
  for (size_t i = 0; i < normalized_pattern.size(); ++i)
    EXPECT_EQ(normalized_pattern[i], notification_data.vibrate[i]);
}

TEST_F(NotificationDataTest, DefaultTimestampValue) {
  NotificationOptions options;

  DummyExceptionStateForTesting exception_state;
  WebNotificationData notification_data = CreateWebNotificationData(
      GetExecutionContext(), kNotificationTitle, options, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  // The timestamp should be set to the current time since the epoch if it
  // wasn't supplied by the developer. "32" has no significance, but an equal
  // comparison of the value could lead to flaky failures.
  EXPECT_NEAR(notification_data.timestamp, WTF::CurrentTimeMS(), 32);
}

TEST_F(NotificationDataTest, DirectionValues) {
  WTF::HashMap<String, WebNotificationData::Direction> mappings;
  mappings.insert("ltr", WebNotificationData::kDirectionLeftToRight);
  mappings.insert("rtl", WebNotificationData::kDirectionRightToLeft);
  mappings.insert("auto", WebNotificationData::kDirectionAuto);

  // Invalid values should default to "auto".
  mappings.insert("peter", WebNotificationData::kDirectionAuto);

  for (const String& direction : mappings.Keys()) {
    NotificationOptions options;
    options.setDir(direction);

    DummyExceptionStateForTesting exception_state;
    WebNotificationData notification_data = CreateWebNotificationData(
        GetExecutionContext(), kNotificationTitle, options, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    EXPECT_EQ(mappings.at(direction), notification_data.direction);
  }
}

TEST_F(NotificationDataTest, MaximumActionCount) {
  HeapVector<NotificationAction> actions;
  for (size_t i = 0; i < Notification::maxActions() + 2; ++i) {
    NotificationAction action;
    action.setAction(String::Number(i));
    action.setTitle(kNotificationActionTitle);

    actions.push_back(action);
  }

  NotificationOptions options;
  options.setActions(actions);

  DummyExceptionStateForTesting exception_state;
  WebNotificationData notification_data = CreateWebNotificationData(
      GetExecutionContext(), kNotificationTitle, options, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // The stored actions will be capped to |maxActions| entries.
  ASSERT_EQ(Notification::maxActions(), notification_data.actions.size());

  for (size_t i = 0; i < Notification::maxActions(); ++i) {
    WebString expected_action = String::Number(i);
    EXPECT_EQ(expected_action, notification_data.actions[i].action);
  }
}

}  // namespace
}  // namespace blink
