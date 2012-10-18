// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#import <SecurityInterface/SFAuthorizationView.h>

#include <string>

#include "base/memory/scoped_ptr.h"
#include "third_party/jsoncpp/source/include/json/value.h"

namespace remoting {

// This is an implementation of JsonHostConfig which does not use code from
// the "base" target, so it can be built for 64-bit on Mac OS X.

// TODO(lambroslambrou): Once the "base" target has 64-bit support, remove this
// implementation and use the one in remoting/host/json_host_config.h - see
// http://crbug.com/128122.
class JsonHostConfig {
 public:
  JsonHostConfig(const std::string& filename);
  ~JsonHostConfig();

  bool Read();
  bool GetString(const std::string& path, std::string* out_value) const;
  std::string GetSerializedData() const;

 private:
  Json::Value config_;
  std::string filename_;

  DISALLOW_COPY_AND_ASSIGN(JsonHostConfig);
};

}

@class Me2MePreferencePaneConfirmPin;
@class Me2MePreferencePaneDisable;

@interface Me2MePreferencePane : NSPreferencePane {
  Me2MePreferencePaneConfirmPin* confirm_pin_view_;
  Me2MePreferencePaneDisable* disable_view_;

  IBOutlet NSTextField* status_message_;
  IBOutlet NSBox* box_;
  IBOutlet SFAuthorizationView* authorization_view_;

  // Holds the new proposed configuration if a temporary config file is
  // present.
  scoped_ptr<remoting::JsonHostConfig> config_;

  NSTimer* service_status_timer_;

  // These flags determine the UI state.  These are computed in the
  // update...Status methods.
  BOOL is_service_running_;
  BOOL is_pane_unlocked_;

  // True if a new proposed config file has been loaded into memory.
  BOOL have_new_config_;

  // True if launchd has been instructed to stop the service and we are waiting
  // for the operation to complete.
  BOOL awaiting_service_stop_;

  // True if a version-mismatch has been detected.  If true, this causes all
  // controls to be greyed out, and also prevents any config file from being
  // deleted, pending a restart of the preference pane.
  BOOL restart_pending_or_canceled_;
}

- (void)mainViewDidLoad;
- (void)willSelect;
- (void)didSelect;
- (void)willUnselect;
- (void)onDisable:(id)sender;
- (void)applyConfiguration:(id)sender
                       pin:(NSString*)pin;
- (void)onNewConfigFile:(NSNotification*)notification;
- (void)refreshServiceStatus:(NSTimer*)timer;
- (void)authorizationViewDidAuthorize:(SFAuthorizationView*)view;
- (void)authorizationViewDidDeauthorize:(SFAuthorizationView*)view;
- (void)updateServiceStatus;
- (void)updateAuthorizationStatus;

// Read any new config file if present.  If a config file is successfully read,
// this deletes the file and keeps the config data loaded in memory.  If this
// method is called a second time (when the file has been deleted), the current
// config is remembered, so this method acts as a latch: it can change
// |have_new_config_| from NO to YES, but never from YES to NO.
//
// This scheme means that this method can delete the file immediately (to avoid
// leaving a stale file around in case of a crash), but this method can safely
// be called multiple times without forgetting the loaded config.  To explicitly
// forget the current config, set |have_new_config_| to NO.
//
// This method should not be called if |restart_pending_or_canceled_| is YES,
// since this would delete any config file.
- (void)readNewConfig;

// Update all UI controls according to any stored flags and loaded config.
// This should be called after any sequence of operations that might change the
// UI state.
- (void)updateUI;

// Alert the user to a generic error condition.
- (void)showError;

// Alert the user that the typed PIN is incorrect.
- (void)showIncorrectPinMessage;

// Save the new config to the system, and either start the service or inform
// the currently-running service of the new config.
- (void)applyNewServiceConfig;

- (BOOL)runHelperAsRootWithCommand:(const char*)command
                         inputData:(const std::string&)input_data;
- (BOOL)sendJobControlMessage:(const char*)launch_key;

// Compare the version of the running pref-pane against the installed version.
// If the versions are mismatched and the pref-pane is visible, disable the
// pane to prevent interaction, and prompt the user to restart System
// Preferences.
//
// This should be called on notification of a new config, and also in
// |didSelect| when the pane becomes visible.  The pane needs to be visible so
// that the alert appears as a sheet over the pane (instead of a detached
// window), which gives the user an appropriate context for the alert.
//
// In the case of a version-mismatch, the new config file should be kept until
// System Preferences is restarted, or thrown away when the user cancels the
// alert.  This method sets the |restart_pending_or_canceled_| flag on
// detecting version-mismatch.
- (void)checkInstalledVersion;

- (void)mismatchAlertDidEnd:(NSAlert*)alert
                 returnCode:(NSInteger)returnCode
                contextInfo:(void*)contextInfo;

// Called when the user chooses OK when prompted to restart System Preferences.
- (void)restartSystemPreferences;

@end
