// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_COCOA_SETTINGS_CONTROLLER_H_
#define UI_MESSAGE_CENTER_COCOA_SETTINGS_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notifier_settings.h"

@class MCSettingsController;

namespace message_center {

// Bridge class between C++ and Cocoa world.
class NotifierSettingsObserverMac : public NotifierSettingsObserver {
 public:
  NotifierSettingsObserverMac(MCSettingsController* settings_controller)
      : settings_controller_(settings_controller) {}
  virtual ~NotifierSettingsObserverMac();

  // Overridden from NotifierSettingsObserver:
  virtual void UpdateIconImage(const std::string& id,
                               const gfx::Image& icon) OVERRIDE;
  virtual void UpdateFavicon(const GURL& url, const gfx::Image& icon) OVERRIDE;

 private:
  MCSettingsController* settings_controller_;  // weak, owns this

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsObserverMac);
};

}  // namespace message_center


// The view controller responsible for the settings sheet in the center.
MESSAGE_CENTER_EXPORT
@interface MCSettingsController : NSViewController {
 @private
  scoped_ptr<message_center::NotifierSettingsObserverMac> observer_;
  message_center::NotifierSettingsProvider* provider_;

  // The "Settings" text at the top.
  base::scoped_nsobject<NSTextField> settingsText_;

  // The smaller text below the "Settings" text.
  base::scoped_nsobject<NSTextField> detailsText_;

  // Container for all the checkboxes.
  base::scoped_nsobject<NSScrollView> scrollView_;

  std::vector<message_center::Notifier*> notifiers_;
}

// Designated initializer.
- (id)initWithProvider:(message_center::NotifierSettingsProvider*)provider;

@end

// Testing API /////////////////////////////////////////////////////////////////

@interface MCSettingsController (TestingAPI)
- (NSScrollView*)scrollView;
@end

#endif  // UI_MESSAGE_CENTER_COCOA_SETTINGS_CONTROLLER_H_
