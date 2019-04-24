// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/thumbnail.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/mac/scoped_nsautorelease_pool.h"
#include "components/history/core/test/thumbnail-inl.h"
#include "ui/gfx/image/image.h"

namespace history {

gfx::Image CreateGoogleThumbnailForTest() {
  base::mac::ScopedNSAutoreleasePool pool;
  // -[NSData dataWithBytesNoCopy:length:freeWhenDone:] takes it first parameter
  // as a void* but does not modify it (API is not const clean) so we need to
  // use const_cast<> here.
  NSData* data =
      [NSData dataWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(
                                      kGoogleThumbnail))
                           length:sizeof(kGoogleThumbnail)
                     freeWhenDone:NO];
  UIImage* image = [UIImage imageWithData:data scale:1];
  return gfx::Image([image retain]);
}

}  // namespace
