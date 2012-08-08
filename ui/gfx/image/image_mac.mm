// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/memory/scoped_nsobject.h"

namespace gfx {
namespace internal {

void PNGFromNSImage(NSImage* nsimage, std::vector<unsigned char>* png) {
  CGImageRef cg_image = [nsimage CGImageForProposedRect:NULL
                                                context:nil
                                                  hints:nil];
  scoped_nsobject<NSBitmapImageRep> ns_bitmap(
      [[NSBitmapImageRep alloc] initWithCGImage:cg_image]);
  NSData* ns_data = [ns_bitmap representationUsingType:NSPNGFileType
                                            properties:nil];
  const unsigned char* bytes =
      static_cast<const unsigned char*>([ns_data bytes]);
  png->assign(bytes, bytes + [ns_data length]);
}

NSImage* NSImageFromPNG(const std::vector<unsigned char>& png) {
  scoped_nsobject<NSData> ns_data(
      [[NSData alloc] initWithBytes:&png.front() length:png.size()]);
  scoped_nsobject<NSImage> image([[NSImage alloc] initWithData:ns_data]);
  if (!image) {
    LOG(WARNING) << "Unable to decode PNG.";
    // Return a 16x16 red image to visually show error.
    NSRect rect = NSMakeRect(0, 0, 16, 16);
    image.reset([[NSImage alloc] initWithSize:rect.size]);
    [image lockFocus];
    [[NSColor colorWithDeviceRed:1.0 green:0.0 blue:0.0 alpha:1.0] set];
    NSRectFill(rect);
    [image unlockFocus];
  }
  return image.release();
}

} // namespace internal
} // namespace gfx

