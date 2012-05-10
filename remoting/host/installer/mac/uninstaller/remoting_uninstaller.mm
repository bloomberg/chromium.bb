// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/mac/uninstaller/remoting_uninstaller.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_authorizationref.h"
#include "base/mac/scoped_cftyperef.h"

@implementation RemotingUninstallerAppDelegate

NSString* const kLaunchAgentsDir = @"/Library/LaunchAgents";
NSString* const kHelperToolsDir = @"/Library/PrivilegedHelperTools";
NSString* const kApplicationDir = @"/Applications";

NSString* const kServiceName = @"org.chromium.chromoting";
NSString* const kUninstallerName =
    @"Chrome Remote Desktop Host Uninstaller.app";

// Keystone
NSString* const kKeystoneAdmin = @"/Library/Google/GoogleSoftwareUpdate/"
                                   "GoogleSoftwareUpdate.bundle/Contents/MacOS/"
                                   "ksadmin";
NSString* const kKeystonePID = @"com.google.chrome_remote_desktop";

- (void)dealloc {
  [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
}

- (void)logOutput:(FILE*) pipe {
  char readBuffer[128];
  for (;;) {
    long bytesRead = read(fileno(pipe), readBuffer, sizeof(readBuffer) - 1);
    if (bytesRead < 1)
      break;
    readBuffer[bytesRead] = '\0';
    NSLog(@"%s", readBuffer);
  }
}

- (void)messageBox:(const char*)message {
  base::mac::ScopedCFTypeRef<CFStringRef> message_ref(
      CFStringCreateWithCString(NULL, message, (int)strlen(message)));
  CFOptionFlags result;
  CFUserNotificationDisplayAlert(0, kCFUserNotificationNoteAlertLevel,
                                 NULL, NULL, NULL,
                                 CFSTR("Chrome Remote Desktop Uninstaller"),
                                 message_ref, NULL, NULL, NULL, &result);
}

- (void)sudoDelete:(const char*)filename
         usingAuth:(AuthorizationRef)authRef  {

  NSLog(@"Executing (as Admin) rm -rf %s", filename);
  const char* tool = "/bin/rm";
  const char* args[] = {"-rf", filename, NULL};
  FILE* pipe = NULL;
  OSStatus status;
  status = AuthorizationExecuteWithPrivileges(authRef, tool,
                                              kAuthorizationFlagDefaults,
                                              (char* const*)args,
                                              &pipe);

  if (status == errAuthorizationToolExecuteFailure) {
    NSLog(@"Error errAuthorizationToolExecuteFailure");
  } else if (status != errAuthorizationSuccess) {
    NSLog(@"Error while executing rm. Status=%lx", status);
  } else {
    [self logOutput:pipe];
  }

  if (pipe != NULL)
    fclose(pipe);
}

-(void)runCommand:(NSString*)cmd
    withArguments:(NSArray*)args {
  NSTask* task;
  NSPipe* output = [NSPipe pipe];
  NSString* result;

  NSLog(@"Executing: %@ %@", cmd, [args componentsJoinedByString:@" "]);

  @try {
    task = [[[NSTask alloc] init] autorelease];
    [task setLaunchPath:cmd];
    [task setArguments:args];
    [task setStandardInput:[NSPipe pipe]];
    [task setStandardOutput:output];
    [task launch];

    NSData* data = [[output fileHandleForReading] readDataToEndOfFile];

    [task waitUntilExit];

    if ([task terminationStatus] != 0) {
      // TODO(garykac): When we switch to sdk_10.6, show the
      // [task terminationReason] as well.
      NSLog(@"Command terminated status=%d", [task terminationStatus]);
    }

    result = [[[NSString alloc] initWithData:data
                                    encoding:NSUTF8StringEncoding]
              autorelease];
    if ([result length] != 0) {
      NSLog(@"Result: %@", result);
    }
  }
  @catch (NSException* exception) {
    NSLog(@"Exception %@ %@", [exception name], [exception reason]);
  }
}

-(void)shutdownService {
  NSString* launchCtl = @"/bin/launchctl";
  NSArray* argsStop = [NSArray arrayWithObjects:@"stop",
                      kServiceName, nil];
  [self runCommand:launchCtl withArguments:argsStop];

  NSString* plist = [NSString stringWithFormat:@"%@/%@.plist",
                     kLaunchAgentsDir, kServiceName];
  if ([[NSFileManager defaultManager] fileExistsAtPath:plist]) {
    NSArray* argsUnload = [NSArray arrayWithObjects:@"unload",
                           @"-w", @"-S", @"Aqua", plist, nil];
    [self runCommand:launchCtl withArguments:argsUnload];
  }
}

-(void)keystoneUnregister {
  NSArray* args = [NSArray arrayWithObjects:@"--delete",
                  @"--productid", kKeystonePID, nil];
  [self runCommand:kKeystoneAdmin withArguments:args];
}

-(void)remotingUninstallUsingAuth:(AuthorizationRef)authRef {
  NSString* host_enabled = [NSString stringWithFormat:@"%@/%@.me2me_enabled",
                            kHelperToolsDir, kServiceName];
  [self sudoDelete:[host_enabled UTF8String] usingAuth:authRef];

  [self shutdownService];

  NSString* plist = [NSString stringWithFormat:@"%@/%@.plist",
                     kLaunchAgentsDir, kServiceName];
  [self sudoDelete:[plist UTF8String] usingAuth:authRef];

  NSString* host_binary = [NSString stringWithFormat:@"%@/%@.me2me_host.app",
                           kHelperToolsDir, kServiceName];
  [self sudoDelete:[host_binary UTF8String] usingAuth:authRef];

  NSString* host_script = [NSString stringWithFormat:@"%@/%@.me2me.sh",
                           kHelperToolsDir, kServiceName];
  [self sudoDelete:[host_script UTF8String] usingAuth:authRef];

  NSString* auth = [NSString stringWithFormat:@"%@/%@.json",
                    kHelperToolsDir, kServiceName];
  [self sudoDelete:[auth UTF8String] usingAuth:authRef];

  NSString* uninstaller = [NSString stringWithFormat:@"%@/%@",
                           kApplicationDir, kUninstallerName];
  [self sudoDelete:[uninstaller UTF8String] usingAuth:authRef];

  [self keystoneUnregister];
}

- (IBAction)uninstall:(NSButton*)sender {
  base::mac::ScopedAuthorizationRef authRef;

  NSLog(@"Chrome Remote Desktop uninstall starting.");

  @try {
    OSStatus status;
    status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
                                 kAuthorizationFlagDefaults, &authRef);
    if (status != errAuthorizationSuccess) {
      [NSException raise:@"AuthorizationCreate Failure"
          format:@"Error during AuthorizationCreate status=%ld", status];
    }

    AuthorizationItem right = {kAuthorizationRightExecute, 0, NULL, 0};
    AuthorizationRights rights = {1, &right};
    AuthorizationFlags flags = kAuthorizationFlagDefaults |
                               kAuthorizationFlagInteractionAllowed |
                               kAuthorizationFlagPreAuthorize |
                               kAuthorizationFlagExtendRights;
    status = AuthorizationCopyRights(authRef, &rights, NULL, flags, NULL);
    if (status == errAuthorizationCanceled) {
      NSLog(@"Chrome Remote Desktop Host uninstall canceled.");
      const char* message = "Chrome Remote Desktop Host uninstall canceled.";
      [self messageBox:message];
    } else if (status == errAuthorizationSuccess) {
      [self remotingUninstallUsingAuth:authRef];

      NSLog(@"Chrome Remote Desktop Host uninstall complete.");
      const char* message =
          "Chrome Remote Desktop Host was successfully uninstalled.";
      [self messageBox:message];
    } else {
      [NSException raise:@"AuthorizationCopyRights Failure"
          format:@"Error during AuthorizationCopyRights status=%ld", status];
    }
  }
  @catch (NSException* exception) {
    NSLog(@"Exception %@ %@", [exception name], [exception reason]);
    const char* message =
        "Error! Unable to uninstall Chrome Remote Desktop Host.";
    [self messageBox:message];
  }

  [NSApp terminate:self];
}

- (IBAction)cancel:(id)sender {
  [NSApp terminate:self];
}

- (IBAction)handleMenuClose:(NSMenuItem*)sender {
  [NSApp terminate:self];
}

@end

int main(int argc, char* argv[])
{
  return NSApplicationMain(argc, (const char**)argv);
}

