/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <libgen.h>
#include <string.h>
#include <unistd.h>

#import <Cocoa/Cocoa.h>

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

namespace nacl {

// For OSX we get the bundle pathname to the browser plugin or the main bundle.
// The returned pointer refers to static storage, so this function is not
// reentrant, but that's okay.
void PluginSelLdrLocator::GetDirectory(char* buffer, size_t len) {
  // Guard our temporary objects below with our own autorelease pool.
  // (We cannot guarantee this is being called from a thread with a pool.)
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  // Identify the path the plugin was loaded from.
  // Create the identifier the build specified in Info.plist.
  NSString* ident = @"com.google.npGoogleNaClPlugin";
  // Find the executable path for that bundle.
  NSString* nspath = [[NSBundle bundleWithIdentifier:ident]
                      pathForResource:@"sel_ldr" ofType:nil];
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
