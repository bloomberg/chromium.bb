// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/nsimage_cache.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

namespace gfx {

static NSMutableDictionary* image_cache = nil;

NSImage* GetCachedImageWithName(NSString* name) {
  DCHECK(name);

  // NOTE: to make this thread safe, we'd have to sync on the cache and
  // also force all the bundle calls on the main thread.

  if (!image_cache) {
    image_cache = [[NSMutableDictionary alloc] init];
    DCHECK(image_cache);
  }

  NSImage* result = [image_cache objectForKey:name];
  if (!result) {
    DVLOG_IF(1, [[name pathExtension] length] == 0) << "Suggest including the "
        "extension in the image name";

    NSString* path = [base::mac::FrameworkBundle() pathForImageResource:name];
    if (path) {
      @try {
        result = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
        if (result) {
          // Auto-template images with names ending in "Template".
          NSString* extensionlessName = [name stringByDeletingPathExtension];
          if ([extensionlessName hasSuffix:@"Template"])
            [result setTemplate:YES];

          [image_cache setObject:result forKey:name];
        }
      }
      @catch (id err) {
        DLOG(ERROR) << "Failed to load the image for name '"
            << [name UTF8String] << "' from path '" << [path UTF8String]
            << "', error: " << [[err description] UTF8String];
        result = nil;
      }
    }
  }

  // TODO: if we ever limit the cache size, this should retain & autorelease
  // the image.
  return result;
}

void ClearCachedImages(void) {
  // NOTE: to make this thread safe, we'd have to sync on the cache.
  [image_cache removeAllObjects];
}

}  // namespace gfx
