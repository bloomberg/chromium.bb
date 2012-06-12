// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/me2me_preference_pane.h"

#import <Cocoa/Cocoa.h>
#include <CommonCrypto/CommonHMAC.h>
#include <launch.h>
#import <PreferencePanes/PreferencePanes.h>
#import <SecurityInterface/SFAuthorizationView.h>
#include <unistd.h>

#include <fstream>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/mac/authorization_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/launchd.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_launch_data.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "remoting/host/host_config.h"
#import "remoting/host/me2me_preference_pane_confirm_pin.h"
#import "remoting/host/me2me_preference_pane_disable.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/writer.h"
#include "third_party/modp_b64/modp_b64.h"

namespace {
// The name of the Remoting Host service that is registered with launchd.
#define kServiceName "org.chromium.chromoting"

// Use separate named notifications for success and failure because sandboxed
// components can't include a dictionary when sending distributed notifications.
// The preferences panel is not yet sandboxed, but err on the side of caution.
#define kUpdateSucceededNotificationName kServiceName ".update_succeeded"
#define kUpdateFailedNotificationName kServiceName ".update_failed"

#define kConfigDir "/Library/PrivilegedHelperTools/"

// This helper script is executed as root.  It is passed a command-line option
// (--enable or --disable), which causes it to create or remove a file that
// informs the host's launch script of whether the host is enabled or disabled.
const char kHelperTool[] = kConfigDir kServiceName ".me2me.sh";

bool GetTemporaryConfigFilePath(std::string* path) {
  NSString* filename = NSTemporaryDirectory();
  if (filename == nil)
    return false;

  filename = [filename stringByAppendingString:@"/" kServiceName ".json"];
  *path = [filename UTF8String];
  return true;
}

bool IsConfigValid(const remoting::JsonHostConfig* config) {
  std::string value;
  return (config->GetString(remoting::kHostIdConfigPath, &value) &&
          config->GetString(remoting::kHostSecretHashConfigPath, &value) &&
          config->GetString(remoting::kXmppLoginConfigPath, &value));
}

bool IsPinValid(const std::string& pin, const std::string& host_id,
                const std::string& host_secret_hash) {
  // TODO(lambroslambrou): Once the "base" target supports building for 64-bit
  // on Mac OS X, remove this code and replace it with |VerifyHostPinHash()|
  // from host/pin_hash.h.
  size_t separator = host_secret_hash.find(':');
  if (separator == std::string::npos)
    return false;

  std::string method = host_secret_hash.substr(0, separator);
  if (method != "hmac") {
    LOG(ERROR) << "Authentication method '" << method << "' not supported";
    return false;
  }

  std::string hash_base64 = host_secret_hash.substr(separator + 1);

  // Convert |hash_base64| to |hash|, based on code from base/base64.cc.
  int hash_base64_size = static_cast<int>(hash_base64.size());
  std::string hash;
  hash.resize(modp_b64_decode_len(hash_base64_size));

  // modp_b64_decode_len() returns at least 1, so hash[0] is safe here.
  int hash_size = modp_b64_decode(&(hash[0]), hash_base64.data(),
                                  hash_base64_size);
  if (hash_size < 0) {
    LOG(ERROR) << "Failed to parse host_secret_hash";
    return false;
  }
  hash.resize(hash_size);

  std::string computed_hash;
  computed_hash.resize(CC_SHA256_DIGEST_LENGTH);

  CCHmac(kCCHmacAlgSHA256,
         host_id.data(), host_id.size(),
         pin.data(), pin.size(),
         &(computed_hash[0]));

  // Normally, a constant-time comparison function would be used, but it is
  // unnecessary here as the "secret" is already readable by the user
  // supplying input to this routine.
  return computed_hash == hash;
}

}  // namespace

namespace remoting {
JsonHostConfig::JsonHostConfig(const std::string& filename)
    : filename_(filename) {
}

JsonHostConfig::~JsonHostConfig() {
}

bool JsonHostConfig::Read() {
  std::ifstream file(filename_.c_str());
  Json::Reader reader;
  return reader.parse(file, config_, false /* ignore comments */);
}

bool JsonHostConfig::GetString(const std::string& path,
                               std::string* out_value) const {
  if (!config_.isObject())
    return false;

  if (!config_.isMember(path))
    return false;

  Json::Value value = config_[path];
  if (!value.isString())
    return false;

  *out_value = value.asString();
  return true;
}

std::string JsonHostConfig::GetSerializedData() const {
  Json::FastWriter writer;
  return writer.write(config_);
}

}  // namespace remoting

@implementation Me2MePreferencePane

- (void)mainViewDidLoad {
  [authorization_view_ setDelegate:self];
  [authorization_view_ setString:kAuthorizationRightExecute];
  [authorization_view_ setAutoupdate:YES];
  confirm_pin_view_ = [[Me2MePreferencePaneConfirmPin alloc] init];
  [confirm_pin_view_ setDelegate:self];
  disable_view_ = [[Me2MePreferencePaneDisable alloc] init];
  [disable_view_ setDelegate:self];
}

- (void)willSelect {
  have_new_config_ = NO;
  awaiting_service_stop_ = NO;

  NSDistributedNotificationCenter* center =
      [NSDistributedNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(onNewConfigFile:)
                 name:@kServiceName
               object:nil];

  service_status_timer_ =
      [[NSTimer scheduledTimerWithTimeInterval:2.0
                                        target:self
                                      selector:@selector(refreshServiceStatus:)
                                      userInfo:nil
                                       repeats:YES] retain];
  [self updateServiceStatus];
  [self updateAuthorizationStatus];

  [self checkInstalledVersion];
  if (!restart_pending_or_canceled_)
    [self readNewConfig];

  [self updateUI];
}

- (void)didSelect {
  [self checkInstalledVersion];
}

- (void)willUnselect {
  NSDistributedNotificationCenter* center =
      [NSDistributedNotificationCenter defaultCenter];
  [center removeObserver:self];

  [service_status_timer_ invalidate];
  [service_status_timer_ release];
  service_status_timer_ = nil;
  if (have_new_config_)
    [self notifyPlugin:kUpdateFailedNotificationName];
}

- (void)applyConfiguration:(id)sender
                       pin:(NSString*)pin {
  if (!have_new_config_) {
    // It shouldn't be possible to hit the button if there is no config to
    // apply, but check anyway just in case it happens somehow.
    return;
  }

  // Ensure the authorization token is up-to-date before using it.
  [self updateAuthorizationStatus];
  [self updateUI];

  std::string pin_utf8 = base::SysNSStringToUTF8(pin);
  std::string host_id, host_secret_hash;
  bool result = (config_->GetString(remoting::kHostIdConfigPath, &host_id) &&
                 config_->GetString(remoting::kHostSecretHashConfigPath,
                                    &host_secret_hash));
  DCHECK(result);
  if (!IsPinValid(pin_utf8, host_id, host_secret_hash)) {
    [self showIncorrectPinMessage];
    return;
  }

  [self applyNewServiceConfig];
  [self updateUI];
}

- (void)onDisable:(id)sender {
  // Ensure the authorization token is up-to-date before using it.
  [self updateAuthorizationStatus];
  [self updateUI];
  if (!is_pane_unlocked_)
    return;

  if (![self runHelperAsRootWithCommand:"--disable"
                              inputData:""]) {
    LOG(ERROR) << "Failed to run the helper tool";
    [self showError];
    [self notifyPlugin: kUpdateFailedNotificationName];
    return;
  }

  // Stop the launchd job.  This cannot easily be done by the helper tool,
  // since the launchd job runs in the current user's context.
  [self sendJobControlMessage:LAUNCH_KEY_STOPJOB];
  awaiting_service_stop_ = YES;
}

- (void)onNewConfigFile:(NSNotification*)notification {
  [self checkInstalledVersion];
  if (!restart_pending_or_canceled_)
    [self readNewConfig];

  [self updateUI];
}

- (void)refreshServiceStatus:(NSTimer*)timer {
  BOOL was_running = is_service_running_;
  [self updateServiceStatus];
  if (awaiting_service_stop_ && !is_service_running_) {
    awaiting_service_stop_ = NO;
    [self notifyPlugin:kUpdateSucceededNotificationName];
  }

  if (was_running != is_service_running_)
    [self updateUI];
}

- (void)authorizationViewDidAuthorize:(SFAuthorizationView*)view {
  [self updateAuthorizationStatus];
  [self updateUI];
}

- (void)authorizationViewDidDeauthorize:(SFAuthorizationView*)view {
  [self updateAuthorizationStatus];
  [self updateUI];
}

- (void)updateServiceStatus {
  pid_t job_pid = base::mac::PIDForJob(kServiceName);
  is_service_running_ = (job_pid > 0);
}

- (void)updateAuthorizationStatus {
  is_pane_unlocked_ = [authorization_view_ updateStatus:authorization_view_];
}

- (void)readNewConfig {
  std::string file;
  if (!GetTemporaryConfigFilePath(&file)) {
    LOG(ERROR) << "Failed to get path of configuration data.";
    [self showError];
    return;
  }
  if (access(file.c_str(), F_OK) != 0)
    return;

  scoped_ptr<remoting::JsonHostConfig> new_config_(
      new remoting::JsonHostConfig(file));
  if (!new_config_->Read()) {
    // Report the error, because the file exists but couldn't be read.  The
    // case of non-existence is normal and expected.
    LOG(ERROR) << "Error reading configuration data from " << file.c_str();
    [self showError];
    return;
  }
  remove(file.c_str());
  if (!IsConfigValid(new_config_.get())) {
    LOG(ERROR) << "Invalid configuration data read.";
    [self showError];
    return;
  }

  config_.swap(new_config_);
  have_new_config_ = YES;

  [confirm_pin_view_ resetPin];
}

- (void)updateUI {
  if (have_new_config_) {
    [box_ setContentView:[confirm_pin_view_ view]];
  } else {
    [box_ setContentView:[disable_view_ view]];
  }

  // TODO(lambroslambrou): Show "enabled" and "disabled" in bold font.
  NSString* message;
  if (is_service_running_) {
    if (have_new_config_) {
      message = @"Please confirm your new PIN.";
    } else {
      message = @"Remote connections to this computer are enabled.";
    }
  } else {
    if (have_new_config_) {
      message = @"Remote connections to this computer are disabled. To enable "
          "remote connections you must confirm your PIN.";
    } else {
      message = @"Remote connections to this computer are disabled.";
    }
  }
  [status_message_ setStringValue:message];

  std::string email;
  if (config_.get()) {
    bool result = config_->GetString(remoting::kXmppLoginConfigPath, &email);

    // The config has already been checked by |IsConfigValid|.
    DCHECK(result);
  }

  [disable_view_ setEnabled:(is_pane_unlocked_ && is_service_running_ &&
                             !restart_pending_or_canceled_)];
  [confirm_pin_view_ setEnabled:(is_pane_unlocked_ &&
                                 !restart_pending_or_canceled_)];
  [confirm_pin_view_ setEmail:base::SysUTF8ToNSString(email)];
  NSString* applyButtonText = is_service_running_ ? @"Confirm" : @"Enable";
  [confirm_pin_view_ setButtonText:applyButtonText];

  if (restart_pending_or_canceled_)
    [authorization_view_ setEnabled:NO];
}

- (void)showError {
  NSAlert* alert = [[NSAlert alloc] init];
  [alert setMessageText:@"An unexpected error occurred."];
  [alert setInformativeText:@"Check the system log for more information."];
  [alert setAlertStyle:NSWarningAlertStyle];
  [alert beginSheetModalForWindow:[[self mainView] window]
                    modalDelegate:nil
                   didEndSelector:nil
                      contextInfo:nil];
  [alert release];
}

- (void)showIncorrectPinMessage {
  NSAlert* alert = [[NSAlert alloc] init];
  [alert setMessageText:@"Incorrect PIN entered."];
  [alert setAlertStyle:NSWarningAlertStyle];
  [alert beginSheetModalForWindow:[[self mainView] window]
                    modalDelegate:nil
                   didEndSelector:nil
                      contextInfo:nil];
  [alert release];
}

- (void)applyNewServiceConfig {
  [self updateServiceStatus];
  std::string serialized_config = config_->GetSerializedData();
  const char* command = is_service_running_ ? "--save-config" : "--enable";
  if (![self runHelperAsRootWithCommand:command
                              inputData:serialized_config]) {
    LOG(ERROR) << "Failed to run the helper tool";
    [self showError];
    return;
  }

  have_new_config_ = NO;

  // If the service is running, send a signal to cause it to reload its
  // configuration, otherwise start the service.
  if (is_service_running_) {
    pid_t job_pid = base::mac::PIDForJob(kServiceName);
    if (job_pid > 0) {
      kill(job_pid, SIGHUP);
    } else {
      LOG(ERROR) << "Failed to obtain PID of service " << kServiceName;
      [self showError];
    }
  } else {
    [self sendJobControlMessage:LAUNCH_KEY_STARTJOB];
  }

  // Broadcast a distributed notification to inform the plugin that the
  // configuration has been applied.
  [self notifyPlugin: kUpdateSucceededNotificationName];
}

- (BOOL)runHelperAsRootWithCommand:(const char*)command
                         inputData:(const std::string&)input_data {
  AuthorizationRef authorization =
      [[authorization_view_ authorization] authorizationRef];
  if (!authorization) {
    LOG(ERROR) << "Failed to obtain authorizationRef";
    return NO;
  }

  // TODO(lambroslambrou): Replace the deprecated ExecuteWithPrivileges
  // call with a launchd-based helper tool, which is more secure.
  // http://crbug.com/120903
  const char* arguments[] = { command, NULL };
  FILE* pipe = NULL;
  pid_t pid;
  OSStatus status = base::mac::ExecuteWithPrivilegesAndGetPID(
      authorization,
      kHelperTool,
      kAuthorizationFlagDefaults,
      arguments,
      &pipe,
      &pid);
  if (status != errAuthorizationSuccess) {
    OSSTATUS_LOG(ERROR, status) << "AuthorizationExecuteWithPrivileges";
    return NO;
  }
  if (pid == -1) {
    LOG(ERROR) << "Failed to get child PID";
    if (pipe)
      fclose(pipe);

    return NO;
  }
  if (!pipe) {
    LOG(ERROR) << "Unexpected NULL pipe";
    return NO;
  }

  // Some cleanup is needed (closing the pipe and waiting for the child
  // process), so flag any errors before returning.
  BOOL error = NO;

  if (!input_data.empty()) {
    size_t bytes_written = fwrite(input_data.data(), sizeof(char),
                                  input_data.size(), pipe);
    // According to the fwrite manpage, a partial count is returned only if a
    // write error has occurred.
    if (bytes_written != input_data.size()) {
      LOG(ERROR) << "Failed to write data to child process";
      error = YES;
    }
  }

  // In all cases, fclose() should be called with the returned FILE*.  In the
  // case of sending data to the child, this needs to be done before calling
  // waitpid(), since the child reads until EOF on its stdin, so calling
  // waitpid() first would result in deadlock.
  if (fclose(pipe) != 0) {
    PLOG(ERROR) << "fclose";
    error = YES;
  }

  int exit_status;
  pid_t wait_result = HANDLE_EINTR(waitpid(pid, &exit_status, 0));
  if (wait_result != pid) {
    PLOG(ERROR) << "waitpid";
    error = YES;
  }

  // No more cleanup needed.
  if (error)
    return NO;

  if (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0) {
    return YES;
  } else {
    LOG(ERROR) << kHelperTool << " failed with exit status " << exit_status;
    return NO;
  }
}

- (BOOL)sendJobControlMessage:(const char*)launch_key {
  base::mac::ScopedLaunchData response(
      base::mac::MessageForJob(kServiceName, launch_key));
  if (!response) {
    LOG(ERROR) << "Failed to send message to launchd";
    [self showError];
    return NO;
  }

  // Expect a response of type LAUNCH_DATA_ERRNO.
  launch_data_type_t type = launch_data_get_type(response.get());
  if (type != LAUNCH_DATA_ERRNO) {
    LOG(ERROR) << "launchd returned unexpected type: " << type;
    [self showError];
    return NO;
  }

  int error = launch_data_get_errno(response.get());
  if (error) {
    LOG(ERROR) << "launchd returned error: " << error;
    [self showError];
    return NO;
  }
  return YES;
}

- (void)notifyPlugin:(const char*)message {
  NSDistributedNotificationCenter* center =
      [NSDistributedNotificationCenter defaultCenter];
  NSString* name = [NSString stringWithUTF8String:message];
  [center postNotificationName:name
                        object:nil
                      userInfo:nil];
}

- (void)checkInstalledVersion {
  // There's no point repeating the check if the pane has already been disabled
  // from a previous call to this method.  The pane only gets disabled when a
  // version-mismatch has been detected here, so skip the check, but continue to
  // handle the version-mismatch case.
  if (!restart_pending_or_canceled_) {
    NSBundle* this_bundle = [NSBundle bundleForClass:[self class]];
    NSDictionary* this_plist = [this_bundle infoDictionary];
    NSString* this_version = [this_plist objectForKey:@"CFBundleVersion"];

    NSString* bundle_path = [this_bundle bundlePath];
    NSString* plist_path =
        [bundle_path stringByAppendingString:@"/Contents/Info.plist"];
    NSDictionary* disk_plist =
        [NSDictionary dictionaryWithContentsOfFile:plist_path];
    NSString* disk_version = [disk_plist objectForKey:@"CFBundleVersion"];

    if (disk_version == nil) {
      LOG(ERROR) << "Failed to get installed version information";
      [self showError];
      return;
    }

    if ([this_version isEqualToString:disk_version])
      return;

    restart_pending_or_canceled_ = YES;
    [self updateUI];
  }

  NSWindow* window = [[self mainView] window];
  if (window == nil) {
    // Defer the alert until |didSelect| is called, which happens just after
    // the window is created.
    return;
  }

  // This alert appears as a sheet over the top of the Chromoting pref-pane,
  // underneath the title, so it's OK to refer to "this preference pane" rather
  // than repeat the title "Chromoting" here.
  NSAlert* alert = [[NSAlert alloc] init];
  [alert setMessageText:@"System update detected"];
  [alert setInformativeText:@"To use this preference pane, System Preferences "
      "needs to be restarted"];
  [alert addButtonWithTitle:@"OK"];
  NSButton* cancel_button = [alert addButtonWithTitle:@"Cancel"];
  [cancel_button setKeyEquivalent:@"\e"];
  [alert setAlertStyle:NSWarningAlertStyle];
  [alert beginSheetModalForWindow:window
                    modalDelegate:self
                   didEndSelector:@selector(
                       mismatchAlertDidEnd:returnCode:contextInfo:)
                      contextInfo:nil];
  [alert release];
}

- (void)mismatchAlertDidEnd:(NSAlert*)alert
                 returnCode:(NSInteger)returnCode
                contextInfo:(void*)contextInfo {
  if (returnCode == NSAlertFirstButtonReturn) {
    // OK was pressed.

    // Dismiss the alert window here, so that the application will respond to
    // the NSApp terminate: message.
    [[alert window] orderOut:nil];
    [self restartSystemPreferences];
  } else {
    // Cancel was pressed.

    // If there is a new config file, delete it and notify the web-app of
    // failure to apply the config.  Otherwise, the web-app will remain in a
    // spinning state until System Preferences eventually gets restarted and
    // the user visits this pane again.
    std::string file;
    if (!GetTemporaryConfigFilePath(&file)) {
      // There's no point in alerting the user here.  The same error would
      // happen when the pane is eventually restarted, so the user would be
      // alerted at that time.
      LOG(ERROR) << "Failed to get path of configuration data.";
      return;
    }
    if (access(file.c_str(), F_OK) != 0)
      return;

    remove(file.c_str());
    [self notifyPlugin:kUpdateFailedNotificationName];
  }
}

- (void)restartSystemPreferences {
  NSTask* task = [[NSTask alloc] init];
  NSArray* arguments = [NSArray arrayWithObjects:@"--relaunch-prefpane", nil];
  [task setLaunchPath:[NSString stringWithUTF8String:kHelperTool]];
  [task setArguments:arguments];
  [task setStandardInput:[NSPipe pipe]];
  [task launch];
  [task release];
  [NSApp terminate:nil];
}

@end
