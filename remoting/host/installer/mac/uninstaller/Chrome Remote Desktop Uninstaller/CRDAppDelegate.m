//
//  CRDAppDelegate.m
//  Chrome Remote Desktop Uninstaller
//
//  Created by Gary Kacmarcik on 4/3/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "CRDAppDelegate.h"

@implementation CRDAppDelegate

@synthesize window = _window;

NSString * const kServiceName = @"org.chromium.chromoting";
NSString * const kLaunchAgents = @"/Library/LaunchAgents";
NSString * const kHelperTools = @"/Library/PrivilegedHelperTools";

// Keystone
NSString * const kKeystoneAdmin = @"/Library/Google/GoogleSoftwareUpdate/"
                                   "GoogleSoftwareUpdate.bundle/Contents/MacOS/"
                                   "ksadmin";
NSString * const kKeystonePID = @"com.google.chrome_remote_desktop";


- (void)dealloc {
    [super dealloc];
}

- (void)applicationDidFinishLaunching: (NSNotification *)aNotification {
}

- (void)logOutput: (FILE *) pipe {
  char readBuffer[128];
  for (;;) {
    long bytesRead = read(fileno(pipe), readBuffer, sizeof(readBuffer)-1);
    if (bytesRead < 1)
      break;
    readBuffer[bytesRead] = '\0';
    NSLog(@"%s", readBuffer);
  }
}

- (void)messageBox: (char *)message {
  CFStringRef message_ref = CFStringCreateWithCString(NULL, message,
                                                      (int)strlen(message));
  CFOptionFlags result;
  CFUserNotificationDisplayAlert(0, kCFUserNotificationNoteAlertLevel,
                                 NULL, NULL, NULL,
                                 CFSTR("Chrome Remote Desktop Uninstaller"),
                                 message_ref, NULL, NULL, NULL, &result);
  CFRelease(message_ref);
}

- (void)sudoDelete:(const char* const)filename
         usingAuth:(AuthorizationRef)authRef  {
  OSStatus status;

  if ([[NSFileManager defaultManager] fileExistsAtPath:@"/bin/rm"]) {
    NSLog(@"rm exists");
  } else {
    NSLog(@"rm doesn't exist");
  }
  if (![[NSFileManager defaultManager]
       fileExistsAtPath:[NSString stringWithFormat:@"%s", filename]]) {
    return;
  }

  NSLog(@"Executing (as Admin) rm -f %s", filename);
  char* tool = "/bin/rm";
  char* const args[] = {"-f", (char* const)filename, NULL};
  FILE* pipe = NULL;
  status = AuthorizationExecuteWithPrivileges(authRef, tool,
                                              kAuthorizationFlagDefaults, args,
                                              &pipe);

  // The status 0x8115ffff is endian-swapped 0xffff1581
  // (=errAuthorizationToolExecuteFailure)
  if (status == 0xffff1581 || status == 0x8115ffff) {
    NSLog(@"Error errAuthorizationToolExecuteFailure");
  } else if (status != errAuthorizationSuccess) {
    NSLog(@"Error while executing rm. Status=%x", status);
  } else {
    [self logOutput: pipe];
  }

  if ([[NSFileManager defaultManager] fileExistsAtPath:@"/bin/rm"]) {
    NSLog(@"rm exists");
  } else {
    NSLog(@"rm doesn't exist");
  }
}

-(void)runCommand:(NSString*)cmd
    withArguments:(NSArray*)args {
  NSTask* task;
  NSPipe* output = [NSPipe pipe];
  NSString* result;

  NSLog(@"Executing: %@ %@", cmd, [args componentsJoinedByString:@" "]);

  @try {
    task = [[NSTask alloc] init];
    [task setLaunchPath: cmd];
    [task setArguments: args];
    [task setStandardInput: [NSPipe pipe]];
    [task setStandardOutput: output];
    [task launch];

    NSData* data = [[output fileHandleForReading] readDataToEndOfFile];

    [task waitUntilExit];

    if ([task terminationStatus] != 0) {
      NSLog(@"Command terminated status=%d reason=%ld",
            [task terminationStatus], [task terminationReason]);
    }

    result = [[NSString alloc] initWithData:data
                                   encoding:NSUTF8StringEncoding];
    if ([result length] != 0) {
      NSLog(@"%@", result);
    }
  }
  @catch (NSException *exception) {
    NSLog(@"Exception %@ %@", [exception name], [exception reason]);
  }

  [task release];
  [result release];
}

-(void)shutdownService {
  NSArray* argsStop = [NSArray arrayWithObjects: @"stop",
                      kServiceName, nil];
  [self runCommand: @"/bin/launchctl" withArguments: argsStop];

  NSString* plist = [NSString stringWithFormat:@"%@/%@.plist", kLaunchAgents,
                     kServiceName];
  if ([[NSFileManager defaultManager] fileExistsAtPath:plist]) {
    NSArray* argsUnload = [NSArray arrayWithObjects: @"unload",
                           @"-w", @"-S", @"Aqua", plist, nil];
    [self runCommand: @"/bin/launchctl" withArguments: argsUnload];
  }
}

-(void)keystoneUnregister {
  NSArray* args = [NSArray arrayWithObjects: @"--delete",
                  @"--productid", kKeystonePID, nil];
  [self runCommand: kKeystoneAdmin withArguments: args];
}

- (IBAction)uninstall: (NSButton *)sender {
  AuthorizationRef authRef;
  OSStatus status;
  bool success = false;

  NSLog(@"Chrome Remote Desktop uninstall starting.");

  @try {
    status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
                                 kAuthorizationFlagDefaults, &authRef);
    if (status != errAuthorizationSuccess)
      NSLog(@"Error during AuthorizationCreate status=%d", status);

    AuthorizationItem right = {kAuthorizationRightExecute, 0, NULL, 0};
    AuthorizationRights rights = {1, &right};
    AuthorizationFlags flags = kAuthorizationFlagDefaults |
                               kAuthorizationFlagInteractionAllowed |
                               kAuthorizationFlagPreAuthorize |
                               kAuthorizationFlagExtendRights;
    status = AuthorizationCopyRights(authRef, &rights, NULL, flags, NULL);
    if (status != errAuthorizationSuccess)
      NSLog(@"Error during AuthorizationCopyRights status=%d", status);

    NSString* host_enabled = [NSString stringWithFormat:@"%@/%@.me2me_enabled",
                              kHelperTools, kServiceName];
    [self sudoDelete: [host_enabled UTF8String] usingAuth: authRef];

    [self shutdownService];

    NSString* plist = [NSString stringWithFormat:@"%@/%@.plist",
                       kLaunchAgents, kServiceName];
    [self sudoDelete: [plist UTF8String] usingAuth: authRef];

    NSString* host_binary = [NSString stringWithFormat:@"%@/%@.me2me_host",
                             kHelperTools, kServiceName];
    [self sudoDelete: [host_binary UTF8String] usingAuth: authRef];

    NSString* host_script = [NSString stringWithFormat:@"%@/%@.me2me.sh",
                             kHelperTools, kServiceName];
    [self sudoDelete: [host_script UTF8String] usingAuth: authRef];

    NSString* auth = [NSString stringWithFormat:@"%@/%@.json",
                      kHelperTools, kServiceName];
    [self sudoDelete: [auth UTF8String] usingAuth: authRef];

    [self keystoneUnregister];

    success = true;
  }
  @catch (NSException *exception) {
    NSLog(@"Exception %@ %@", [exception name], [exception reason]);
  }

  @finally {
    status = AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
  }

  NSLog(@"Chrome Remote Desktop uninstall complete.");
  if (success)
    [self messageBox:"Chrome Remote Desktop was successfully uninstalled."];

  [NSApp terminate:self];
}

- (IBAction)cancel: (id)sender {
  [NSApp terminate: self];
}

- (IBAction)handleMenuClose: (NSMenuItem *)sender {
  [NSApp terminate: self];
}

@end
