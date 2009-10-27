/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#import <Cocoa/Cocoa.h>

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

namespace nacl {

// For OSX we get the bundle pathname to the browser plugin or the main bundle.
// The returned pointer refers to static storage, so this function is not
// reentrant, but that's okay.
const char* SelLdrLauncher::GetPluginDirname() {
  // Semantics for all of the OS-dependent implementations of this
  // function is that a static buffer is used, and caller need not
  // deallocate.  Not thread safe.
  static char system_buffer[MAXPATHLEN + 1];

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
  // And strip off the dll name, returning only the path, or NULL if error.
  if (NULL == pathname) {
    [pool release];
    return NULL;
  } else {
    strncpy(system_buffer,  pathname, sizeof system_buffer - 1);
    system_buffer[sizeof system_buffer - 1] = '\0';
    [pool release];
    return dirname(system_buffer);
  }
}

}  // namespace nacl
