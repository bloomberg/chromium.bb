// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/me2me_preference_pane.h"

#import <Cocoa/Cocoa.h>
#include <CommonCrypto/CommonHMAC.h>
#include <launch.h>
#import <PreferencePanes/PreferencePanes.h>
#import <SecurityInterface/SFAuthorizationView.h>

#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/file_util.h"
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
#include "remoting/host/json_host_config.h"
#include "third_party/modp_b64/modp_b64.h"

namespace {
// The name of the Remoting Host service that is registered with launchd.
#define kServiceName "org.chromium.chromoting"
#define kConfigDir "/Library/PrivilegedHelperTools/"

// This helper script is executed as root.  It is passed a command-line option
// (--enable or --disable), which causes it to create or remove a file that
// informs the host's launch script of whether the host is enabled or disabled.
const char kHelperTool[] = kConfigDir kServiceName ".me2me.sh";

bool GetTemporaryConfigFilePath(FilePath* path) {
  if (!file_util::GetTempDir(path))
    return false;
  *path = path->Append(kServiceName ".json");
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

  return computed_hash == hash;
}

}  // namespace


@implementation Me2MePreferencePane

- (void)mainViewDidLoad {
  [authorization_view_ setDelegate:self];
  [authorization_view_ setString:kAuthorizationRightExecute];
  [authorization_view_ setAutoupdate:YES];
}

- (void)willSelect {
  have_new_config_ = NO;

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
  [self readNewConfig];
  [self updateUI];
}

- (void)willUnselect {
  NSDistributedNotificationCenter* center =
      [NSDistributedNotificationCenter defaultCenter];
  [center removeObserver:self];

  [service_status_timer_ invalidate];
  [service_status_timer_ release];
  service_status_timer_ = nil;
}

- (void)onApply:(id)sender {
  if (!have_new_config_) {
    // It shouldn't be possible to hit the button if there is no config to
    // apply, but check anyway just in case it happens somehow.
    return;
  }

  // Ensure the authorization token is up-to-date before using it.
  [self updateAuthorizationStatus];
  [self updateUI];

  std::string pin = base::SysNSStringToUTF8([pin_ stringValue]);
  std::string host_id, host_secret_hash;
  bool result = (config_->GetString(remoting::kHostIdConfigPath, &host_id) &&
                 config_->GetString(remoting::kHostSecretHashConfigPath,
                                    &host_secret_hash));
  DCHECK(result);
  if (!IsPinValid(pin, host_id, host_secret_hash)) {
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
    return;
  }

  // Stop the launchd job.  This cannot easily be done by the helper tool,
  // since the launchd job runs in the current user's context.
  [self sendJobControlMessage:LAUNCH_KEY_STOPJOB];
}

- (void)onNewConfigFile:(NSNotification*)notification {
  [self readNewConfig];
  [self updateUI];
}

- (void)refreshServiceStatus:(NSTimer*)timer {
  BOOL was_running = is_service_running_;
  [self updateServiceStatus];
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
  FilePath file;
  if (!GetTemporaryConfigFilePath(&file)) {
    LOG(ERROR) << "Failed to get path of configuration data.";
    [self showError];
    return;
  }
  if (!file_util::PathExists(file))
    return;

  scoped_ptr<remoting::JsonHostConfig> new_config_(
      new remoting::JsonHostConfig(file));
  if (!new_config_->Read()) {
    // Report the error, because the file exists but couldn't be read.  The
    // case of non-existence is normal and expected.
    LOG(ERROR) << "Error reading configuration data from " << file.value();
    [self showError];
    return;
  }
  file_util::Delete(file, false);
  if (!IsConfigValid(new_config_.get())) {
    LOG(ERROR) << "Invalid configuration data read.";
    [self showError];
    return;
  }

  config_.swap(new_config_);
  have_new_config_ = YES;
}

- (void)updateUI {
  // TODO(lambroslambrou): These strings should be localized.
#ifdef OFFICIAL_BUILD
  NSString* name = @"Chrome Remote Desktop";
#else
  NSString* name = @"Chromoting";
#endif
  NSString* message;
  if (is_service_running_) {
    message = [NSString stringWithFormat:@"%@ is enabled", name];
  } else {
    message = [NSString stringWithFormat:@"%@ is disabled", name];
  }
  [status_message_ setStringValue:message];

  std::string email;
  if (config_.get()) {
    bool result = config_->GetString(remoting::kXmppLoginConfigPath, &email);

    // The config has already been checked by |IsConfigValid|.
    DCHECK(result);
  }
  [email_ setStringValue:base::SysUTF8ToNSString(email)];

  [disable_button_ setEnabled:(is_pane_unlocked_ && is_service_running_)];
  [pin_instruction_message_ setEnabled:have_new_config_];
  [email_ setEnabled:have_new_config_];
  [pin_ setEnabled:have_new_config_];
  [apply_button_ setEnabled:(is_pane_unlocked_ && have_new_config_)];
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

@end
