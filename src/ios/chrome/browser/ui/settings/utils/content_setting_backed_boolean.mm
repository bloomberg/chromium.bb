// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"

#include "base/scoped_observer.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSettingBackedBoolean ()

// The ID of the setting in |settingsMap|.
@property(nonatomic, readonly) ContentSettingsType settingID;

// Whether the boolean value reflects the state of the preference that backs it,
// or its negation.
@property(nonatomic, assign, getter=isInverted) BOOL inverted;

// Whether this object is the one modifying the content setting. Used to filter
// out changes notifications.
@property(nonatomic, assign) BOOL isModifyingContentSetting;

@end

namespace {

typedef ScopedObserver<HostContentSettingsMap, content_settings::Observer>
    ContentSettingsObserver;

class ContentSettingsObserverBridge : public content_settings::Observer {
 public:
  explicit ContentSettingsObserverBridge(ContentSettingBackedBoolean* setting);
  ~ContentSettingsObserverBridge() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

 private:
  ContentSettingBackedBoolean* setting_;  // weak
};

ContentSettingsObserverBridge::ContentSettingsObserverBridge(
    ContentSettingBackedBoolean* setting)
    : setting_(setting) {}

ContentSettingsObserverBridge::~ContentSettingsObserverBridge() {}

void ContentSettingsObserverBridge::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  // Ignore when it's the ContentSettingBackedBoolean that is changing the
  // content setting.
  if (setting_.isModifyingContentSetting) {
    return;
  }
  const ContentSettingsDetails settings_details(
      primary_pattern, secondary_pattern, content_type, resource_identifier);
  ContentSettingsType settingID = settings_details.type();
  // Unfortunately, because the ContentSettingsPolicyProvider doesn't publish
  // the specific content setting on policy updates, we must refresh on every
  // ContentSettingsType::DEFAULT notification.
  if (settingID != ContentSettingsType::DEFAULT &&
      settingID != setting_.settingID) {
    return;
  }
  // Notify the BooleanObserver.
  [setting_.observer booleanDidChange:setting_];
}

}  // namespace

@implementation ContentSettingBackedBoolean {
  ContentSettingsType _settingID;
  scoped_refptr<HostContentSettingsMap> _settingsMap;
  std::unique_ptr<ContentSettingsObserverBridge> _adaptor;
  std::unique_ptr<ContentSettingsObserver> _content_settings_observer;
}

@synthesize settingID = _settingID;
@synthesize observer = _observer;
@synthesize inverted = _inverted;
@synthesize isModifyingContentSetting = _isModifyingContentSetting;

- (id)initWithHostContentSettingsMap:(HostContentSettingsMap*)settingsMap
                           settingID:(ContentSettingsType)settingID
                            inverted:(BOOL)inverted {
  self = [super init];
  if (self) {
    _settingID = settingID;
    _settingsMap = settingsMap;
    _inverted = inverted;
    // Listen for changes to the content setting.
    _adaptor.reset(new ContentSettingsObserverBridge(self));
    _content_settings_observer.reset(
        new ContentSettingsObserver(_adaptor.get()));
    _content_settings_observer->Add(settingsMap);
  }
  return self;
}

- (BOOL)value {
  ContentSetting setting =
      _settingsMap->GetDefaultContentSetting(_settingID, NULL);
  return self.inverted ^ (setting == CONTENT_SETTING_ALLOW);
}

- (void)setValue:(BOOL)value {
  ContentSetting setting =
      (self.inverted ^ value) ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  self.isModifyingContentSetting = YES;
  _settingsMap->SetDefaultContentSetting(_settingID, setting);
  self.isModifyingContentSetting = NO;
}

@end
