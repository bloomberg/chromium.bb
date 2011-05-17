// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_mac.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "skia/ext/bitmap_platform_device_mac.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

// 10.6 API that we use if available.
#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6

@interface NSImageRep (NSObject)

- (BOOL)drawInRect:(NSRect)dstSpacePortionRect
          fromRect:(NSRect)srcSpacePortionRect
         operation:(NSCompositingOperation)op
          fraction:(CGFloat)requestedAlpha
    respectFlipped:(BOOL)respectContextIsFlipped
             hints:(NSDictionary*)hints;

@end

#endif // OSes < Mac OS X 10.6

namespace {

// Draws an NSImage or an NSImageRep with a given size into a SkBitmap.
SkBitmap NSImageOrNSImageRepToSkBitmap(
    NSImage* image,
    NSImageRep* image_rep,
    NSSize size,
    bool is_opaque) {
  // Only image or image_rep should be provided, not both.
  DCHECK((image != 0) ^ (image_rep != 0));

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width, size.height);
  if (!bitmap.allocPixels())
    return bitmap;  // Return |bitmap| which should respond true to isNull().

  bitmap.setIsOpaque(is_opaque);

  base::mac::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  void* data = bitmap.getPixels();

  // Allocate a bitmap context with 4 components per pixel (BGRA). Apple
  // recommends these flags for improved CG performance.
#define HAS_ARGB_SHIFTS(a, r, g, b) \
            (SK_A32_SHIFT == (a) && SK_R32_SHIFT == (r) \
             && SK_G32_SHIFT == (g) && SK_B32_SHIFT == (b))
#if defined(SK_CPU_LENDIAN) && HAS_ARGB_SHIFTS(24, 16, 8, 0)
  base::mac::ScopedCFTypeRef<CGContextRef> context(
    CGBitmapContextCreate(data, size.width, size.height, 8, size.width*4,
                          color_space,
                          kCGImageAlphaPremultipliedFirst |
                              kCGBitmapByteOrder32Host));
#else
#error We require that Skia's and CoreGraphics's recommended \
       image memory layout match.
#endif
#undef HAS_ARGB_SHIFTS

  // Something went really wrong. Best guess is that the bitmap data is invalid.
  DCHECK(context);

  // Save the current graphics context so that we can restore it later.
  gfx::ScopedNSGraphicsContextSaveGState scoped_g_state;

  // Dummy context that we will draw into.
  NSGraphicsContext* context_cocoa =
      [NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO];
  [NSGraphicsContext setCurrentContext:context_cocoa];

  // This will stretch any images to |size| if it does not fit or is non-square.
  NSRect drawRect = NSMakeRect(0, 0, size.width, size.height);

  // NSImage does caching such that subsequent drawing is much faster (on my
  // machine, about 4x faster). Unfortunately on 10.5 NSImageRep doesn't do
  // caching. For this reason we draw using an NSImage if available. Once
  // 10.5 is no longer supported we can drop this and always use NSImageRep.
  if (image) {
    [image drawInRect:drawRect
             fromRect:NSZeroRect
            operation:NSCompositeCopy
             fraction:1.0];
  } else {
    // Use NSCompositeCopy if available, it's slightly faster.
    if ([image_rep respondsToSelector:@selector(
        drawInRect:fromRect:operation:fraction:respectFlipped:hints:)]) {
      [image_rep drawInRect:drawRect
                   fromRect:NSZeroRect
                  operation:NSCompositeCopy
                   fraction:1.0
             respectFlipped:NO
                      hints:NO];
    } else {
      [image_rep drawInRect:drawRect];
    }
  }

  return bitmap;
}

} // namespace

namespace gfx {

CGAffineTransform SkMatrixToCGAffineTransform(const SkMatrix& matrix) {
  // CGAffineTransforms don't support perspective transforms, so make sure
  // we don't get those.
  DCHECK(matrix[SkMatrix::kMPersp0] == 0.0f);
  DCHECK(matrix[SkMatrix::kMPersp1] == 0.0f);
  DCHECK(matrix[SkMatrix::kMPersp2] == 1.0f);

  return CGAffineTransformMake(matrix[SkMatrix::kMScaleX],
                               matrix[SkMatrix::kMSkewY],
                               matrix[SkMatrix::kMSkewX],
                               matrix[SkMatrix::kMScaleY],
                               matrix[SkMatrix::kMTransX],
                               matrix[SkMatrix::kMTransY]);
}

SkIRect CGRectToSkIRect(const CGRect& rect) {
  SkIRect sk_rect = {
    SkScalarRound(rect.origin.x),
    SkScalarRound(rect.origin.y),
    SkScalarRound(rect.origin.x + rect.size.width),
    SkScalarRound(rect.origin.y + rect.size.height)
  };
  return sk_rect;
}

SkRect CGRectToSkRect(const CGRect& rect) {
  SkRect sk_rect = {
    rect.origin.x,
    rect.origin.y,
    rect.origin.x + rect.size.width,
    rect.origin.y + rect.size.height,
  };
  return sk_rect;
}

CGRect SkIRectToCGRect(const SkIRect& rect) {
  CGRect cg_rect = {
    { rect.fLeft, rect.fTop },
    { rect.fRight - rect.fLeft, rect.fBottom - rect.fTop }
  };
  return cg_rect;
}

CGRect SkRectToCGRect(const SkRect& rect) {
  CGRect cg_rect = {
    { rect.fLeft, rect.fTop },
    { rect.fRight - rect.fLeft, rect.fBottom - rect.fTop }
  };
  return cg_rect;
}

// Converts CGColorRef to the ARGB layout Skia expects.
SkColor CGColorRefToSkColor(CGColorRef color) {
  DCHECK(CGColorGetNumberOfComponents(color) == 4);
  const CGFloat* components = CGColorGetComponents(color);
  return SkColorSetARGB(SkScalarRound(255.0 * components[3]), // alpha
                        SkScalarRound(255.0 * components[0]), // red
                        SkScalarRound(255.0 * components[1]), // green
                        SkScalarRound(255.0 * components[2])); // blue
}

// Converts ARGB to CGColorRef.
CGColorRef SkColorToCGColorRef(SkColor color) {
  return CGColorCreateGenericRGB(SkColorGetR(color) / 255.0,
                                 SkColorGetG(color) / 255.0,
                                 SkColorGetB(color) / 255.0,
                                 SkColorGetA(color) / 255.0);
}

SkBitmap CGImageToSkBitmap(CGImageRef image) {
  if (!image)
    return SkBitmap();

  int width = CGImageGetWidth(image);
  int height = CGImageGetHeight(image);

  scoped_ptr<skia::BitmapPlatformDevice> device(
      skia::BitmapPlatformDevice::Create(NULL, width, height, false));

  CGContextRef context = device->GetBitmapContext();

  // We need to invert the y-axis of the canvas so that Core Graphics drawing
  // happens right-side up. Skia has an upper-left origin and CG has a lower-
  // left one.
  CGContextScaleCTM(context, 1.0, -1.0);
  CGContextTranslateCTM(context, 0, -height);

  // We want to copy transparent pixels from |image|, instead of blending it
  // onto uninitialized pixels.
  CGContextSetBlendMode(context, kCGBlendModeCopy);

  CGRect rect = CGRectMake(0, 0, width, height);
  CGContextDrawImage(context, rect, image);

  // Because |device| will be cleaned up and will take its pixels with it, we
  // copy it to the stack and return it.
  SkBitmap bitmap = device->accessBitmap(false);

  return bitmap;
}

SkBitmap NSImageToSkBitmap(NSImage* image, NSSize size, bool is_opaque) {
  return NSImageOrNSImageRepToSkBitmap(image, nil, size, is_opaque);
}

SkBitmap NSImageRepToSkBitmap(NSImageRep* image, NSSize size, bool is_opaque) {
  return NSImageOrNSImageRepToSkBitmap(nil, image, size, is_opaque);
}

NSImage* SkBitmapToNSImageWithColorSpace(const SkBitmap& skiaBitmap,
                                         CGColorSpaceRef colorSpace) {
  if (skiaBitmap.isNull())
    return nil;

  // First convert SkBitmap to CGImageRef.
  base::mac::ScopedCFTypeRef<CGImageRef> cgimage(
      SkCreateCGImageRefWithColorspace(skiaBitmap, colorSpace));

  // Now convert to NSImage.
  scoped_nsobject<NSBitmapImageRep> bitmap(
      [[NSBitmapImageRep alloc] initWithCGImage:cgimage]);
  scoped_nsobject<NSImage> image([[NSImage alloc] init]);
  [image addRepresentation:bitmap];
  [image setSize:NSMakeSize(skiaBitmap.width(), skiaBitmap.height())];
  return [image.release() autorelease];
}

NSImage* SkBitmapToNSImage(const SkBitmap& skiaBitmap) {
  base::mac::ScopedCFTypeRef<CGColorSpaceRef> colorSpace(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  return SkBitmapToNSImageWithColorSpace(skiaBitmap, colorSpace.get());
}

NSImage* SkBitmapsToNSImage(const std::vector<const SkBitmap*>& bitmaps) {
  if (bitmaps.empty())
    return nil;

  base::mac::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  scoped_nsobject<NSImage> image([[NSImage alloc] init]);
  NSSize min_size = NSZeroSize;

  for (std::vector<const SkBitmap*>::const_iterator it = bitmaps.begin();
       it != bitmaps.end(); ++it) {
    const SkBitmap& skiaBitmap = **it;
    // First convert SkBitmap to CGImageRef.
    base::mac::ScopedCFTypeRef<CGImageRef> cgimage(
        SkCreateCGImageRefWithColorspace(skiaBitmap, color_space));

    // Now convert to NSImage.
    scoped_nsobject<NSBitmapImageRep> bitmap(
        [[NSBitmapImageRep alloc] initWithCGImage:cgimage]);
    [image addRepresentation:bitmap];

    if (min_size.width == 0 || min_size.width > skiaBitmap.width()) {
      min_size.width = skiaBitmap.width();
      min_size.height = skiaBitmap.height();
    }
  }

  [image setSize:min_size];
  return [image.release() autorelease];
}

SkBitmap AppplicationIconAtSize(int size) {
  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  return NSImageToSkBitmap(image, NSMakeSize(size, size), /* is_opaque=*/true);
}


SkiaBitLocker::SkiaBitLocker(SkCanvas* canvas)
    : canvas_(canvas),
      cgContext_(0) {
}

SkiaBitLocker::~SkiaBitLocker() {
  releaseIfNeeded();
}

void SkiaBitLocker::releaseIfNeeded() {
  if (!cgContext_)
    return;
  canvas_->getDevice()->accessBitmap(true).unlockPixels();
  CGContextRelease(cgContext_);
  cgContext_ = 0;
}

CGContextRef SkiaBitLocker::cgContext() {
  SkDevice* device = canvas_->getDevice();
  DCHECK(device);
  if (!device)
    return 0;
  releaseIfNeeded();
  const SkBitmap& bitmap = device->accessBitmap(true);
  bitmap.lockPixels();
  void* pixels = bitmap.getPixels();
  cgContext_ = CGBitmapContextCreate(pixels, device->width(),
    device->height(), 8, bitmap.rowBytes(), CGColorSpaceCreateDeviceRGB(), 
    kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);

  // Apply device matrix.
  CGAffineTransform contentsTransform = CGAffineTransformMakeScale(1, -1);
  contentsTransform = CGAffineTransformTranslate(contentsTransform, 0,
      -device->height());
  CGContextConcatCTM(cgContext_, contentsTransform);

  // Apply clip in device coordinates.
  CGMutablePathRef clipPath = CGPathCreateMutable();
  SkRegion::Iterator iter(canvas_->getTotalClip());
  for (; !iter.done(); iter.next()) {
    const SkIRect& skRect = iter.rect();
    CGRect cgRect = SkIRectToCGRect(skRect);
    CGPathAddRect(clipPath, 0, cgRect);
  }
  CGContextAddPath(cgContext_, clipPath);
  CGContextClip(cgContext_);
  CGPathRelease(clipPath);

  // Apply content matrix.
  const SkMatrix& skMatrix = canvas_->getTotalMatrix();
  CGAffineTransform affine = SkMatrixToCGAffineTransform(skMatrix);
  CGContextConcatCTM(cgContext_, affine);
  
  return cgContext_;
}

}  // namespace gfx
