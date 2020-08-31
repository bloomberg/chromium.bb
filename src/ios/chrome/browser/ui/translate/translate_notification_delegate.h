// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_DELEGATE_H_

#import "ios/chrome/browser/ui/translate/translate_notification_handler.h"

// Protocol adopted by an object that gets notified when the translate
// notification UI is dismissed, automatically or by the user.
@protocol TranslateNotificationDelegate

// Tells the delegate that the notification of the given type was dismissed,
// either automatically or by the user.
- (void)translateNotificationHandlerDidDismiss:
            (id<TranslateNotificationHandler>)sender
                              notificationType:(TranslateNotificationType)type;

// Tells the delegate that the notification of the given type was undone
// by the user.
- (void)translateNotificationHandlerDidUndo:
            (id<TranslateNotificationHandler>)sender
                           notificationType:(TranslateNotificationType)type;

// The source language name.
@property(nonatomic, readonly) NSString* sourceLanguage;

// The target language name.
@property(nonatomic, readonly) NSString* targetLanguage;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_DELEGATE_H_
