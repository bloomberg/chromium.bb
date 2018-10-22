//
//  GTMSystemVersion.m
//
//  Copyright 2007-2008 Google Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not
//  use this file except in compliance with the License.  You may obtain a copy
//  of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
//  License for the specific language governing permissions and limitations under
//  the License.
//

#import "GTMSystemVersion.h"

#import <objc/message.h>

#if GTM_MACOS_SDK
#import <CoreServices/CoreServices.h>
#else
// On iOS we cheat and pull in the header for UIDevice to get the selectors,
// but call it via runtime since GTMSystemVersion is supposed to only depend on
// Foundation.
#import "UIKit/UIDevice.h"
#endif

static SInt32 sGTMSystemVersionMajor = 0;
static SInt32 sGTMSystemVersionMinor = 0;
static SInt32 sGTMSystemVersionBugFix = 0;
static NSString *sBuild = nil;

NSString *const kGTMArch_iPhone = @"iPhone";
NSString *const kGTMArch_ppc = @"ppc";
NSString *const kGTMArch_ppc64 = @"ppc64";
NSString *const kGTMArch_x86_64 = @"x86_64";
NSString *const kGTMArch_i386 = @"i386";

static NSString *const kSystemVersionPlistPath = @"/System/Library/CoreServices/SystemVersion.plist";

@implementation GTMSystemVersion
+ (void)initialize {
  if (self == [GTMSystemVersion class]) {
    // Gestalt is the recommended way of getting the OS version (despite a
    // comment to the contrary in the 10.4 headers and docs; see
    // <http://lists.apple.com/archives/carbon-dev/2007/Aug/msg00089.html>).
    // The iPhone doesn't have Gestalt though, so use the plist there.
#if GTM_MACOS_SDK
    __Require_noErr(Gestalt(gestaltSystemVersionMajor,
                            &sGTMSystemVersionMajor), failedGestalt);
    __Require_noErr(Gestalt(gestaltSystemVersionMinor,
                            &sGTMSystemVersionMinor), failedGestalt);
    __Require_noErr(Gestalt(gestaltSystemVersionBugFix,
                            &sGTMSystemVersionBugFix), failedGestalt);

    return;

  failedGestalt:
    ;
#else // GTM_MACOS_SDK
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSString *version = nil;

    // The intent is for this file to be Foundation level, so don't directly
    // call out to UIDevice, but try to get it at runtime before falling back
    // to the plist.  The problem with using the plist on the Simulator is that
    // the path will be on the host system, and give us a MacOS (10.x.y)
    // version number instead of an iOS version number.
    Class uideviceClass = NSClassFromString(@"UIDevice");
    if (uideviceClass) {
      id currentDevice = ((id (*)(id, SEL))objc_msgSend)(uideviceClass, @selector(currentDevice));
      version = [currentDevice performSelector:@selector(systemVersion)];
    }
    if (!version) {
      // Fall back to the info in the Plist.
      NSDictionary *systemVersionPlist
        = [NSDictionary dictionaryWithContentsOfFile:kSystemVersionPlistPath];
      version = [systemVersionPlist objectForKey:@"ProductVersion"];
    }
    _GTMDevAssert(version, @"Unable to get version");

    NSArray *versionInfo = [version componentsSeparatedByString:@"."];
    NSUInteger length = [versionInfo count];
    _GTMDevAssert(length > 1 && length < 4,
                  @"Unparseable version %@", version);
    sGTMSystemVersionMajor = [[versionInfo objectAtIndex:0] intValue];
    _GTMDevAssert(sGTMSystemVersionMajor != 0,
                  @"Unknown version for %@", version);
    sGTMSystemVersionMinor = [[versionInfo objectAtIndex:1] intValue];
    if (length == 3) {
      sGTMSystemVersionBugFix = [[versionInfo objectAtIndex:2] intValue];
    }
    [pool release];
#endif // GTM_MACOS_SDK
  }
}

+ (void)getMajor:(SInt32*)major minor:(SInt32*)minor bugFix:(SInt32*)bugFix {
  if (major) {
    *major = sGTMSystemVersionMajor;
  }
  if (minor) {
    *minor = sGTMSystemVersionMinor;
  }
  if (bugFix) {
    *bugFix = sGTMSystemVersionBugFix;
  }
}

+ (NSString*)build {
  @synchronized(self) {
    // Not cached at initialization time because we don't expect "real"
    // software to want this, and it costs a bit to get at startup.
    // This will mainly be for unit test cases.
    if (!sBuild) {
      NSDictionary *systemVersionPlist
        = [NSDictionary dictionaryWithContentsOfFile:kSystemVersionPlistPath];
      sBuild = [[systemVersionPlist objectForKey:@"ProductBuildVersion"] retain];
      _GTMDevAssert(sBuild, @"Unable to get build version");
    }
  }
  return sBuild;
}

+ (BOOL)isBuildLessThan:(NSString*)build {
  NSComparisonResult result
    = [[self build] compare:build
                    options:NSNumericSearch | NSCaseInsensitiveSearch];
  return result == NSOrderedAscending;
}

+ (BOOL)isBuildLessThanOrEqualTo:(NSString*)build {
  NSComparisonResult result
    = [[self build] compare:build
                    options:NSNumericSearch | NSCaseInsensitiveSearch];
  return result != NSOrderedDescending;
}

+ (BOOL)isBuildGreaterThan:(NSString*)build {
  NSComparisonResult result
    = [[self build] compare:build
                    options:NSNumericSearch | NSCaseInsensitiveSearch];
  return result == NSOrderedDescending;
}

+ (BOOL)isBuildGreaterThanOrEqualTo:(NSString*)build {
  NSComparisonResult result
    = [[self build] compare:build
                    options:NSNumericSearch | NSCaseInsensitiveSearch];
  return result != NSOrderedAscending;
}

+ (BOOL)isBuildEqualTo:(NSString *)build {
  NSComparisonResult result
    = [[self build] compare:build
                    options:NSNumericSearch | NSCaseInsensitiveSearch];
  return result == NSOrderedSame;
}

#if GTM_MACOS_SDK
+ (BOOL)isPanther {
  return NO;
}

+ (BOOL)isTiger {
  return NO;
}

+ (BOOL)isLeopard {
  return NO;
}

+ (BOOL)isSnowLeopard {
#if defined(__MAC_10_7) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_7
  return NO;
#else
  return sGTMSystemVersionMajor == 10 && sGTMSystemVersionMinor == 6;
#endif
}

+ (BOOL)isPantherOrGreater {
  return YES;
}

+ (BOOL)isTigerOrGreater {
  return YES;
}

+ (BOOL)isLeopardOrGreater {
  return YES;
}

+ (BOOL)isSnowLeopardOrGreater {
  return YES;
}

#endif // GTM_MACOS_SDK

+ (NSString *)runtimeArchitecture {
  NSString *architecture = nil;
#if GTM_IPHONE_SDK
  architecture = kGTMArch_iPhone;
#else // !GTM_IPHONE_SDK
  // In reading arch(3) you'd thing this would work:
  //
  // const NXArchInfo *localInfo = NXGetLocalArchInfo();
  // _GTMDevAssert(localInfo && localInfo->name, @"Couldn't get NXArchInfo");
  // const NXArchInfo *genericInfo = NXGetArchInfoFromCpuType(localInfo->cputype, 0);
  // _GTMDevAssert(genericInfo && genericInfo->name, @"Couldn't get generic NXArchInfo");
  // extensions[0] = [NSString stringWithFormat:@".%s", genericInfo->name];
  //
  // but on 64bit it returns the same things as on 32bit, so...
#if __POWERPC__
#if __LP64__
  architecture = kGTMArch_ppc64;
#else // !__LP64__
  architecture = kGTMArch_ppc;
#endif // __LP64__
#else // !__POWERPC__
#if __LP64__
  architecture = kGTMArch_x86_64;
#else // !__LP64__
  architecture = kGTMArch_i386;
#endif // __LP64__
#endif // !__POWERPC__

#endif // GTM_IPHONE_SDK
  return architecture;
}

@end
