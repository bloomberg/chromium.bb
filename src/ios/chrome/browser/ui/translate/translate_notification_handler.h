// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_HANDLER_H_

@protocol TranslateNotificationDelegate;

// Types of translate notifications.
typedef NS_ENUM(NSInteger, TranslateNotificationType) {
  // Notification that user chose 'sourceLanguage' to always be translated to
  // 'targetLanguage'.
  TranslateNotificationTypeAlwaysTranslate,
  // Notification that it was automatically decided that 'sourceLanguage' to
  // always be translated to 'targetLanguage'.
  TranslateNotificationTypeAutoAlwaysTranslate,
  // Notification that user chose 'sourceLanguage' not to be translated.
  TranslateNotificationTypeNeverTranslate,
  // Notification that it was automatically decided that 'sourceLanguage' not to
  // be translated.
  TranslateNotificationTypeAutoNeverTranslate,
  // Notification that user chose the site not to be translated.
  TranslateNotificationTypeNeverTranslateSite,
  // Notification that the page could not be translated.
  TranslateNotificationTypeError,
};

// Protocol adopted by an object that displays translate notifications.
@protocol TranslateNotificationHandler

// Tells the handler to display a notification of the given type and inform
// |delegate| of its dismissal (whether automatically or by the user) as well as
// its reversal by the user.
- (void)showTranslateNotificationWithDelegate:
            (id<TranslateNotificationDelegate>)delegate
                             notificationType:(TranslateNotificationType)type;

// Tells the handler to stop displaying the notification, if any.
- (void)dismissNotification;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_HANDLER_H_
