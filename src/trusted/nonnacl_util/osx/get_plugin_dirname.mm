/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <libgen.h>
#include <string.h>
#include <unistd.h>

#import <Cocoa/Cocoa.h>

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

// TODO(polina): we need this to avoid hardcoding the bundle name, but it
// causes the following error when building chrome:
// ld: duplicate symbol nacl::PluginSelLdrLocator::GetDirectory(char*,
// unsigned long) in
// chrome/src/xcodebuild/Debug/libsel_ldr_launcher.a(get_plugin_dirname.o) and
// chrome/src/xcodebuild/Debug/libnonnacl_util_chrome.a(get_plugin_dirname.o)
//
// Dummy class object to be used with bundleForClass below.
//@interface Dummy: NSObject {}
//@end
//@implementation Dummy
//@end

namespace nacl {

// For OSX we get the bundle pathname to the browser plugin or the main bundle.
// The returned pointer refers to static storage, so this function is not
// reentrant, but that's okay.
void PluginSelLdrLocator::GetDirectory(char* buffer, size_t len) {
  // Guard our temporary objects below with our own autorelease pool.
  // (We cannot guarantee this is being called from a thread with a pool.)
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

  // Find the executable path for the bundle the plugin was loaded from.
  // Expect the sel_ldr to be within the bundle directory.
  NSString* ident = @"com.google.npGoogleNaClPlugin";  // Id from Info.plist.
  NSString* nspath = [[NSBundle bundleWithIdentifier:ident]
                      pathForResource:@"sel_ldr" ofType:nil];
  // TODO(polina): enable this instead when the error caused by Dummy is fixed
  // NSString* nspath = [[NSBundle bundleForClass:[Dummy class] ]
  //                     pathForResource:@"sel_ldr" ofType:nil];

  // Convert it to a C string.
  char const* pathname = [nspath fileSystemRepresentation];
  // If we were not able to find the plugin bundle, try the main bundle.
  // This is necessary to support sel_universal, and possibly other apps
  // using the launcher.
  if (NULL == pathname) {
    nspath = [[NSBundle mainBundle] pathForResource:@"sel_ldr" ofType:nil];
    pathname = [nspath fileSystemRepresentation];
  }

  if (NULL == pathname) {
    [pool release];
    buffer[0] = '\0';
  } else {
    // And strip off the dll name, returning only the path, or NULL if error.
    strncpy(buffer,  pathname, len - 1);
    buffer[len - 1] = '\0';
    [pool release];
    char* path_end = strrchr(buffer, '/');
    if (NULL != path_end) {
      *path_end = '\0';
    }
  }
}

}  // namespace nacl
