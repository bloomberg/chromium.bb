// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/mac/me2me_preference_pane.h"

#import <Cocoa/Cocoa.h>
#include <CommonCrypto/CommonHMAC.h>
#include <errno.h>
#include <launch.h>
#import <PreferencePanes/PreferencePanes.h>
#import <SecurityInterface/SFAuthorizationView.h>
#include <stdlib.h>
#include <unistd.h>

#include <fstream>

#include "base/mac/scoped_launch_data.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "remoting/host/constants_mac.h"
#include "remoting/host/host_config.h"
#import "remoting/host/mac/me2me_preference_pane_confirm_pin.h"
#import "remoting/host/mac/me2me_preference_pane_disable.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/writer.h"
#include "third_party/modp_b64/modp_b64.h"

namespace {

bool GetTemporaryConfigFilePath(std::string* path) {
  NSString* filename = NSTemporaryDirectory();
  if (filename == nil)
    return false;

  *path = [[NSString stringWithFormat:@"%@/%s",
            filename, remoting::kHostConfigFileName] UTF8String];
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
    NSLog(@"Authentication method '%s' not supported", method.c_str());
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
    NSLog(@"Failed to parse host_secret_hash");
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

// These methods are copied from base/mac, but with the logging changed to use
// NSLog().
//
// TODO(lambroslambrou): Once the "base" target supports building for 64-bit
// on Mac OS X, remove these implementations and use the ones in base/mac.
namespace base {
namespace mac {

// MessageForJob sends a single message to launchd with a simple dictionary
// mapping |operation| to |job_label|, and returns the result of calling
// launch_msg to send that message. On failure, returns NULL. The caller
// assumes ownership of the returned launch_data_t object.
launch_data_t MessageForJob(const std::string& job_label,
                            const char* operation) {
  // launch_data_alloc returns something that needs to be freed.
  ScopedLaunchData message(launch_data_alloc(LAUNCH_DATA_DICTIONARY));
  if (!message) {
    NSLog(@"launch_data_alloc");
    return NULL;
  }

  // launch_data_new_string returns something that needs to be freed, but
  // the dictionary will assume ownership when launch_data_dict_insert is
  // called, so put it in a scoper and .release() it when given to the
  // dictionary.
  ScopedLaunchData job_label_launchd(launch_data_new_string(job_label.c_str()));
  if (!job_label_launchd) {
    NSLog(@"launch_data_new_string");
    return NULL;
  }

  if (!launch_data_dict_insert(message,
                               job_label_launchd.release(),
                               operation)) {
    return NULL;
  }

  return launch_msg(message);
}

pid_t PIDForJob(const std::string& job_label) {
  ScopedLaunchData response(MessageForJob(job_label, LAUNCH_KEY_GETJOB));
  if (!response) {
    return -1;
  }

  launch_data_type_t response_type = launch_data_get_type(response);
  if (response_type != LAUNCH_DATA_DICTIONARY) {
    if (response_type == LAUNCH_DATA_ERRNO) {
      NSLog(@"PIDForJob: error %d", launch_data_get_errno(response));
    } else {
      NSLog(@"PIDForJob: expected dictionary, got %d", response_type);
    }
    return -1;
  }

  launch_data_t pid_data = launch_data_dict_lookup(response,
                                                   LAUNCH_JOBKEY_PID);
  if (!pid_data)
    return 0;

  if (launch_data_get_type(pid_data) != LAUNCH_DATA_INTEGER) {
    NSLog(@"PIDForJob: expected integer");
    return -1;
  }

  return launch_data_get_integer(pid_data);
}

OSStatus ExecuteWithPrivilegesAndGetPID(AuthorizationRef authorization,
                                        const char* tool_path,
                                        AuthorizationFlags options,
                                        const char** arguments,
                                        FILE** pipe,
                                        pid_t* pid) {
  // pipe may be NULL, but this function needs one.  In that case, use a local
  // pipe.
  FILE* local_pipe;
  FILE** pipe_pointer;
  if (pipe) {
    pipe_pointer = pipe;
  } else {
    pipe_pointer = &local_pipe;
  }

  // AuthorizationExecuteWithPrivileges wants |char* const*| for |arguments|,
  // but it doesn't actually modify the arguments, and that type is kind of
  // silly and callers probably aren't dealing with that.  Put the cast here
  // to make things a little easier on callers.
  OSStatus status = AuthorizationExecuteWithPrivileges(authorization,
                                                       tool_path,
                                                       options,
                                                       (char* const*)arguments,
                                                       pipe_pointer);
  if (status != errAuthorizationSuccess) {
    return status;
  }

  long line_pid = -1;
  size_t line_length = 0;
  char* line_c = fgetln(*pipe_pointer, &line_length);
  if (line_c) {
    if (line_length > 0 && line_c[line_length - 1] == '\n') {
      // line_c + line_length is the start of the next line if there is one.
      // Back up one character.
      --line_length;
    }
    std::string line(line_c, line_length);

    // The version in base/mac used base::StringToInt() here.
    line_pid = strtol(line.c_str(), NULL, 10);
    if (line_pid == 0) {
      NSLog(@"ExecuteWithPrivilegesAndGetPid: funny line: %s", line.c_str());
      line_pid = -1;
    }
  } else {
    NSLog(@"ExecuteWithPrivilegesAndGetPid: no line");
  }

  if (!pipe) {
    fclose(*pipe_pointer);
  }

  if (pid) {
    *pid = line_pid;
  }

  return status;
}

}  // namespace mac
}  // namespace base

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
  [authorization_view_ setAutoupdate:YES
                            interval:60];
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
                 name:[NSString stringWithUTF8String:remoting::kServiceName]
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

  [self notifyPlugin:UPDATE_FAILED_NOTIFICATION_NAME];
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

  std::string pin_utf8 = [pin UTF8String];
  std::string host_id, host_secret_hash;
  bool result = (config_->GetString(remoting::kHostIdConfigPath, &host_id) &&
                 config_->GetString(remoting::kHostSecretHashConfigPath,
                                    &host_secret_hash));
  if (!result) {
    [self showError];
    return;
  }
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
    NSLog(@"Failed to run the helper tool");
    [self showError];
    [self notifyPlugin:UPDATE_FAILED_NOTIFICATION_NAME];
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
    [self notifyPlugin:UPDATE_SUCCEEDED_NOTIFICATION_NAME];
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
  pid_t job_pid = base::mac::PIDForJob(remoting::kServiceName);
  is_service_running_ = (job_pid > 0);
}

- (void)updateAuthorizationStatus {
  is_pane_unlocked_ = [authorization_view_ updateStatus:authorization_view_];
}

- (void)readNewConfig {
  std::string file;
  if (!GetTemporaryConfigFilePath(&file)) {
    NSLog(@"Failed to get path of configuration data.");
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
    NSLog(@"Error reading configuration data from %s", file.c_str());
    [self showError];
    return;
  }
  remove(file.c_str());
  if (!IsConfigValid(new_config_.get())) {
    NSLog(@"Invalid configuration data read.");
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
    bool result = config_->GetString(remoting::kHostOwnerConfigPath, &email);
    if (!result) {
      result = config_->GetString(remoting::kXmppLoginConfigPath, &email);

      // The config has already been checked by |IsConfigValid|.
      if (!result) {
        [self showError];
        return;
      }
    }
  }
  [disable_view_ setEnabled:(is_pane_unlocked_ && is_service_running_ &&
                             !restart_pending_or_canceled_)];
  [confirm_pin_view_ setEnabled:(is_pane_unlocked_ &&
                                 !restart_pending_or_canceled_)];
  [confirm_pin_view_ setEmail:[NSString stringWithUTF8String:email.c_str()]];
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
    NSLog(@"Failed to run the helper tool");
    [self showError];
    return;
  }

  have_new_config_ = NO;

  // Ensure the service is started.
  if (!is_service_running_) {
    [self sendJobControlMessage:LAUNCH_KEY_STARTJOB];
  }

  // Broadcast a distributed notification to inform the plugin that the
  // configuration has been applied.
  [self notifyPlugin:UPDATE_SUCCEEDED_NOTIFICATION_NAME];
}

- (BOOL)runHelperAsRootWithCommand:(const char*)command
                         inputData:(const std::string&)input_data {
  AuthorizationRef authorization =
      [[authorization_view_ authorization] authorizationRef];
  if (!authorization) {
    NSLog(@"Failed to obtain authorizationRef");
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
      remoting::kHostHelperScriptPath,
      kAuthorizationFlagDefaults,
      arguments,
      &pipe,
      &pid);
  if (status != errAuthorizationSuccess) {
    NSLog(@"AuthorizationExecuteWithPrivileges: %s (%d)",
          GetMacOSStatusErrorString(status), static_cast<int>(status));
    return NO;
  }
  if (pid == -1) {
    NSLog(@"Failed to get child PID");
    if (pipe)
      fclose(pipe);

    return NO;
  }
  if (!pipe) {
    NSLog(@"Unexpected NULL pipe");
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
      NSLog(@"Failed to write data to child process");
      error = YES;
    }
  }

  // In all cases, fclose() should be called with the returned FILE*.  In the
  // case of sending data to the child, this needs to be done before calling
  // waitpid(), since the child reads until EOF on its stdin, so calling
  // waitpid() first would result in deadlock.
  if (fclose(pipe) != 0) {
    NSLog(@"fclose failed with error %d", errno);
    error = YES;
  }

  int exit_status;
  pid_t wait_result = HANDLE_EINTR(waitpid(pid, &exit_status, 0));
  if (wait_result != pid) {
    NSLog(@"waitpid failed with error %d", errno);
    error = YES;
  }

  // No more cleanup needed.
  if (error)
    return NO;

  if (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0) {
    return YES;
  } else {
    NSLog(@"%s failed with exit status %d", remoting::kHostHelperScriptPath,
          exit_status);
    return NO;
  }
}

- (BOOL)sendJobControlMessage:(const char*)launch_key {
  base::mac::ScopedLaunchData response(
      base::mac::MessageForJob(remoting::kServiceName, launch_key));
  if (!response) {
    NSLog(@"Failed to send message to launchd");
    [self showError];
    return NO;
  }

  // Expect a response of type LAUNCH_DATA_ERRNO.
  launch_data_type_t type = launch_data_get_type(response.get());
  if (type != LAUNCH_DATA_ERRNO) {
    NSLog(@"launchd returned unexpected type: %d", type);
    [self showError];
    return NO;
  }

  int error = launch_data_get_errno(response.get());
  if (error) {
    NSLog(@"launchd returned error: %d", error);
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
      NSLog(@"Failed to get installed version information");
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
      NSLog(@"Failed to get path of configuration data.");
      return;
    }

    remove(file.c_str());
    [self notifyPlugin:UPDATE_FAILED_NOTIFICATION_NAME];
  }
}

- (void)restartSystemPreferences {
  NSTask* task = [[NSTask alloc] init];
  NSString* command =
      [NSString stringWithUTF8String:remoting::kHostHelperScriptPath];
  NSArray* arguments = [NSArray arrayWithObjects:@"--relaunch-prefpane", nil];
  [task setLaunchPath:command];
  [task setArguments:arguments];
  [task setStandardInput:[NSPipe pipe]];
  [task launch];
  [task release];
  [NSApp terminate:nil];
}

@end
