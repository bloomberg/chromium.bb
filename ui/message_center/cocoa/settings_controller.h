// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_COCOA_SETTINGS_CONTROLLER_H_
#define UI_MESSAGE_CENTER_COCOA_SETTINGS_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notifier_settings.h"

@class MCSettingsController;

namespace message_center {

// Bridge class between C++ and Cocoa world.
class NotifierSettingsDelegateMac : public NotifierSettingsDelegate {
 public:
  NotifierSettingsDelegateMac(MCSettingsController* settings_controller)
      : settings_controller_(settings_controller) {}
  virtual ~NotifierSettingsDelegateMac();

  MCSettingsController* cocoa_controller() { return settings_controller_; }

  // Overridden from NotifierSettingsDelegate:
  virtual void UpdateIconImage(const std::string& id,
                               const gfx::Image& icon) OVERRIDE;
  virtual void UpdateFavicon(const GURL& url, const gfx::Image& icon) OVERRIDE;

 private:
  MCSettingsController* settings_controller_;  // weak, owns this

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsDelegateMac);
};

}  // namespace message_center


// The view controller responsible for the settings sheet in the center.
MESSAGE_CENTER_EXPORT
@interface MCSettingsController : NSViewController {
 @private
  scoped_ptr<message_center::NotifierSettingsDelegateMac> delegate_;
  message_center::NotifierSettingsProvider* provider_;
}

// Designated initializer.
- (id)initWithProvider:(message_center::NotifierSettingsProvider*)provider;

// Returns the bridge object for this controller.
- (message_center::NotifierSettingsDelegateMac*)delegate;

@end

#endif  // UI_MESSAGE_CENTER_COCOA_SETTINGS_CONTROLLER_H_
