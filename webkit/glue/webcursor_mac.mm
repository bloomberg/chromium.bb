// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webcursor.h"

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_nsobject.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "ui/gfx/mac/nsimage_cache.h"

using WebKit::WebCursorInfo;
using WebKit::WebImage;
using WebKit::WebSize;

// Declare symbols that are part of the 10.6 SDK.
#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6

@interface NSCursor (SnowLeopardSDKDeclarations)
+ (NSCursor*)contextualMenuCursor;
+ (NSCursor*)dragCopyCursor;
+ (NSCursor*)operationNotAllowedCursor;
@end

#endif  // MAC_OS_X_VERSION_10_6

// Declare symbols that are part of the 10.7 SDK.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface NSCursor (LionSDKDeclarations)
+ (NSCursor*)IBeamCursorForVerticalLayout;
@end

#endif  // MAC_OS_X_VERSION_10_7

// Private interface to CoreCursor, as of Mac OS X 10.7. This is essentially the
// implementation of WKCursor in WebKitSystemInterface.

enum {
  kArrowCursor = 0,
  kIBeamCursor = 1,
  kMakeAliasCursor = 2,
  kOperationNotAllowedCursor = 3,
  kBusyButClickableCursor = 4,
  kCopyCursor = 5,
  kClosedHandCursor = 11,
  kOpenHandCursor = 12,
  kPointingHandCursor = 13,
  kCountingUpHandCursor = 14,
  kCountingDownHandCursor = 15,
  kCountingUpAndDownHandCursor = 16,
  kResizeLeftCursor = 17,
  kResizeRightCursor = 18,
  kResizeLeftRightCursor = 19,
  kCrosshairCursor = 20,
  kResizeUpCursor = 21,
  kResizeDownCursor = 22,
  kResizeUpDownCursor = 23,
  kContextualMenuCursor = 24,
  kDisappearingItemCursor = 25,
  kVerticalIBeamCursor = 26,
  kResizeEastCursor = 27,
  kResizeEastWestCursor = 28,
  kResizeNortheastCursor = 29,
  kResizeNortheastSouthwestCursor = 30,
  kResizeNorthCursor = 31,
  kResizeNorthSouthCursor = 32,
  kResizeNorthwestCursor = 33,
  kResizeNorthwestSoutheastCursor = 34,
  kResizeSoutheastCursor = 35,
  kResizeSouthCursor = 36,
  kResizeSouthwestCursor = 37,
  kResizeWestCursor = 38,
  kMoveCursor = 39,
  kHelpCursor = 40,  // Present on >= 10.7.3.
  kCellCursor = 41,  // Present on >= 10.7.3.
  kZoomInCursor = 42,  // Present on >= 10.7.3.
  kZoomOutCursor = 43  // Present on >= 10.7.3.
};
typedef long long CrCoreCursorType;

@interface CrCoreCursor : NSCursor {
 @private
  CrCoreCursorType type_;
}

+ (id)cursorWithType:(CrCoreCursorType)type;
- (id)initWithType:(CrCoreCursorType)type;
- (CrCoreCursorType)_coreCursorType;

@end

@implementation CrCoreCursor

+ (id)cursorWithType:(CrCoreCursorType)type {
  NSCursor* cursor = [[CrCoreCursor alloc] initWithType:type];
  if ([cursor image])
    return [cursor autorelease];

  [cursor release];
  return nil;
}

- (id)initWithType:(CrCoreCursorType)type {
  if ((self = [super init])) {
    type_ = type;
  }
  return self;
}

- (CrCoreCursorType)_coreCursorType {
  return type_;
}

@end

namespace {

// Loads a cursor from the image cache.
NSCursor* LoadCursor(const char* name, int hotspot_x, int hotspot_y) {
  NSString* file_name = [NSString stringWithUTF8String:name];
  DCHECK(file_name);
  // TODO: This image fetch can (and probably should) be serviced by the
  // resource resource bundle instead of going through the image cache.
  NSImage* cursor_image = gfx::GetCachedImageWithName(file_name);
  DCHECK(cursor_image);
  return [[[NSCursor alloc] initWithImage:cursor_image
                                  hotSpot:NSMakePoint(hotspot_x,
                                                      hotspot_y)] autorelease];
}

// Gets a specified cursor from CoreCursor, falling back to loading it from the
// image cache if CoreCursor cannot provide it.
NSCursor* GetCoreCursorWithFallback(CrCoreCursorType type,
                                    const char* name,
                                    int hotspot_x,
                                    int hotspot_y) {
  if (base::mac::IsOSLionOrLater()) {
    NSCursor* cursor = [CrCoreCursor cursorWithType:type];
    if (cursor)
      return cursor;
  }

  return LoadCursor(name, hotspot_x, hotspot_y);
}

// TODO(avi): When Skia becomes default, fold this function into the remaining
// caller, InitFromCursor().
CGImageRef CreateCGImageFromCustomData(const std::vector<char>& custom_data,
                                       const gfx::Size& custom_size) {
  // If the data is missing, leave the backing transparent.
  void* data = NULL;
  if (!custom_data.empty()) {
    // This is safe since we're not going to draw into the context we're
    // creating.
    data = const_cast<char*>(&custom_data[0]);
  }

  // If the size is empty, use a 1x1 transparent image.
  gfx::Size size = custom_size;
  if (size.IsEmpty()) {
    size.SetSize(1, 1);
    data = NULL;
  }

  base::mac::ScopedCFTypeRef<CGColorSpaceRef> cg_color(
      CGColorSpaceCreateDeviceRGB());
  // The settings here match SetCustomData() below; keep in sync.
  base::mac::ScopedCFTypeRef<CGContextRef> context(
      CGBitmapContextCreate(data,
                            size.width(),
                            size.height(),
                            8,
                            size.width()*4,
                            cg_color.get(),
                            kCGImageAlphaPremultipliedLast |
                            kCGBitmapByteOrder32Big));
  return CGBitmapContextCreateImage(context.get());
}

NSCursor* CreateCustomCursor(const std::vector<char>& custom_data,
                             const gfx::Size& custom_size,
                             const gfx::Point& hotspot) {
  // If the data is missing, leave the backing transparent.
  void* data = NULL;
  size_t data_size = 0;
  if (!custom_data.empty()) {
    // This is safe since we're not going to draw into the context we're
    // creating.
    data = const_cast<char*>(&custom_data[0]);
    data_size = custom_data.size();
  }

  // If the size is empty, use a 1x1 transparent image.
  gfx::Size size = custom_size;
  if (size.IsEmpty()) {
    size.SetSize(1, 1);
    data = NULL;
  }

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
  bitmap.allocPixels();
  if (data)
    memcpy(bitmap.getAddr32(0, 0), data, data_size);
  else
    bitmap.eraseARGB(0, 0, 0, 0);
  NSImage* cursor_image = gfx::SkBitmapToNSImage(bitmap);

  NSCursor* cursor = [[NSCursor alloc] initWithImage:cursor_image
                                             hotSpot:NSMakePoint(hotspot.x(),
                                                                 hotspot.y())];

  return [cursor autorelease];
}

}  // namespace

// Match Safari's cursor choices; see platform/mac/CursorMac.mm .
gfx::NativeCursor WebCursor::GetNativeCursor() {
  switch (type_) {
    case WebCursorInfo::TypePointer:
      return [NSCursor arrowCursor];
    case WebCursorInfo::TypeCross:
      return [NSCursor crosshairCursor];
    case WebCursorInfo::TypeHand:
      // If >= 10.7, the pointingHandCursor has a shadow so use it. Otherwise
      // use the custom one.
      if (base::mac::IsOSLionOrLater())
        return [NSCursor pointingHandCursor];
      else
        return LoadCursor("linkCursor", 6, 1);
    case WebCursorInfo::TypeIBeam:
      return [NSCursor IBeamCursor];
    case WebCursorInfo::TypeWait:
      return GetCoreCursorWithFallback(kBusyButClickableCursor,
                                       "waitCursor", 7, 7);
    case WebCursorInfo::TypeHelp:
      return GetCoreCursorWithFallback(kHelpCursor,
                                       "helpCursor", 8, 8);
    case WebCursorInfo::TypeEastResize:
    case WebCursorInfo::TypeEastPanning:
      return GetCoreCursorWithFallback(kResizeEastCursor,
                                       "eastResizeCursor", 14, 7);
    case WebCursorInfo::TypeNorthResize:
    case WebCursorInfo::TypeNorthPanning:
      return GetCoreCursorWithFallback(kResizeNorthCursor,
                                       "northResizeCursor", 7, 1);
    case WebCursorInfo::TypeNorthEastResize:
    case WebCursorInfo::TypeNorthEastPanning:
      return GetCoreCursorWithFallback(kResizeNortheastCursor,
                                       "northEastResizeCursor", 14, 1);
    case WebCursorInfo::TypeNorthWestResize:
    case WebCursorInfo::TypeNorthWestPanning:
      return GetCoreCursorWithFallback(kResizeNorthwestCursor,
                                       "northWestResizeCursor", 0, 0);
    case WebCursorInfo::TypeSouthResize:
    case WebCursorInfo::TypeSouthPanning:
      return GetCoreCursorWithFallback(kResizeSouthCursor,
                                       "southResizeCursor", 7, 14);
    case WebCursorInfo::TypeSouthEastResize:
    case WebCursorInfo::TypeSouthEastPanning:
      return GetCoreCursorWithFallback(kResizeSoutheastCursor,
                                       "southEastResizeCursor", 14, 14);
    case WebCursorInfo::TypeSouthWestResize:
    case WebCursorInfo::TypeSouthWestPanning:
      return GetCoreCursorWithFallback(kResizeSouthwestCursor,
                                       "southWestResizeCursor", 1, 14);
    case WebCursorInfo::TypeWestResize:
    case WebCursorInfo::TypeWestPanning:
      return GetCoreCursorWithFallback(kResizeWestCursor,
                                       "westResizeCursor", 1, 7);
    case WebCursorInfo::TypeNorthSouthResize:
      return GetCoreCursorWithFallback(kResizeNorthSouthCursor,
                                       "northSouthResizeCursor", 7, 7);
    case WebCursorInfo::TypeEastWestResize:
      return GetCoreCursorWithFallback(kResizeEastWestCursor,
                                       "eastWestResizeCursor", 7, 7);
    case WebCursorInfo::TypeNorthEastSouthWestResize:
      return GetCoreCursorWithFallback(kResizeNortheastSouthwestCursor,
                                       "northEastSouthWestResizeCursor", 7, 7);
    case WebCursorInfo::TypeNorthWestSouthEastResize:
      return GetCoreCursorWithFallback(kResizeNorthwestSoutheastCursor,
                                       "northWestSouthEastResizeCursor", 7, 7);
    case WebCursorInfo::TypeColumnResize:
      return [NSCursor resizeLeftRightCursor];
    case WebCursorInfo::TypeRowResize:
      return [NSCursor resizeUpDownCursor];
    case WebCursorInfo::TypeMiddlePanning:
    case WebCursorInfo::TypeMove:
      return GetCoreCursorWithFallback(kMoveCursor,
                                       "moveCursor", 7, 7);
    case WebCursorInfo::TypeVerticalText:
      // IBeamCursorForVerticalLayout is >= 10.7.
      if ([NSCursor respondsToSelector:@selector(IBeamCursorForVerticalLayout)])
        return [NSCursor IBeamCursorForVerticalLayout];
      else
        return LoadCursor("verticalTextCursor", 7, 7);
    case WebCursorInfo::TypeCell:
      return GetCoreCursorWithFallback(kCellCursor,
                                       "cellCursor", 7, 7);
    case WebCursorInfo::TypeContextMenu:
      // contextualMenuCursor is >= 10.6.
      if ([NSCursor respondsToSelector:@selector(contextualMenuCursor)])
        return [NSCursor contextualMenuCursor];
      else
        return LoadCursor("contextMenuCursor", 3, 2);
    case WebCursorInfo::TypeAlias:
      return GetCoreCursorWithFallback(kMakeAliasCursor,
                                       "aliasCursor", 11, 3);
    case WebCursorInfo::TypeProgress:
      return GetCoreCursorWithFallback(kBusyButClickableCursor,
                                       "progressCursor", 3, 2);
    case WebCursorInfo::TypeNoDrop:
    case WebCursorInfo::TypeNotAllowed:
      // Docs say that operationNotAllowedCursor is >= 10.6, and it's not in the
      // 10.5 SDK, but later SDKs note that it really is available on 10.5.
      return [NSCursor operationNotAllowedCursor];
    case WebCursorInfo::TypeCopy:
      // dragCopyCursor is >= 10.6.
      if ([NSCursor respondsToSelector:@selector(dragCopyCursor)])
        return [NSCursor dragCopyCursor];
      else
        return LoadCursor("copyCursor", 3, 2);
    case WebCursorInfo::TypeNone:
      return LoadCursor("noneCursor", 7, 7);
    case WebCursorInfo::TypeZoomIn:
      return GetCoreCursorWithFallback(kZoomInCursor,
                                       "zoomInCursor", 7, 7);
    case WebCursorInfo::TypeZoomOut:
      return GetCoreCursorWithFallback(kZoomOutCursor,
                                       "zoomOutCursor", 7, 7);
    case WebCursorInfo::TypeGrab:
      return [NSCursor openHandCursor];
    case WebCursorInfo::TypeGrabbing:
      return [NSCursor closedHandCursor];
    case WebCursorInfo::TypeCustom:
      return CreateCustomCursor(custom_data_, custom_size_, hotspot_);
  }
  NOTREACHED();
  return nil;
}

void WebCursor::InitFromThemeCursor(ThemeCursor cursor) {
  WebKit::WebCursorInfo cursor_info;

  switch (cursor) {
    case kThemeArrowCursor:
      cursor_info.type = WebCursorInfo::TypePointer;
      break;
    case kThemeCopyArrowCursor:
      cursor_info.type = WebCursorInfo::TypeCopy;
      break;
    case kThemeAliasArrowCursor:
      cursor_info.type = WebCursorInfo::TypeAlias;
      break;
    case kThemeContextualMenuArrowCursor:
      cursor_info.type = WebCursorInfo::TypeContextMenu;
      break;
    case kThemeIBeamCursor:
      cursor_info.type = WebCursorInfo::TypeIBeam;
      break;
    case kThemeCrossCursor:
    case kThemePlusCursor:
      cursor_info.type = WebCursorInfo::TypeCross;
      break;
    case kThemeWatchCursor:
    case kThemeSpinningCursor:
      cursor_info.type = WebCursorInfo::TypeWait;
      break;
    case kThemeClosedHandCursor:
      cursor_info.type = WebCursorInfo::TypeGrabbing;
      break;
    case kThemeOpenHandCursor:
      cursor_info.type = WebCursorInfo::TypeGrab;
      break;
    case kThemePointingHandCursor:
    case kThemeCountingUpHandCursor:
    case kThemeCountingDownHandCursor:
    case kThemeCountingUpAndDownHandCursor:
      cursor_info.type = WebCursorInfo::TypeHand;
      break;
    case kThemeResizeLeftCursor:
      cursor_info.type = WebCursorInfo::TypeWestResize;
      break;
    case kThemeResizeRightCursor:
      cursor_info.type = WebCursorInfo::TypeEastResize;
      break;
    case kThemeResizeLeftRightCursor:
      cursor_info.type = WebCursorInfo::TypeEastWestResize;
      break;
    case kThemeNotAllowedCursor:
      cursor_info.type = WebCursorInfo::TypeNotAllowed;
      break;
    case kThemeResizeUpCursor:
      cursor_info.type = WebCursorInfo::TypeNorthResize;
      break;
    case kThemeResizeDownCursor:
      cursor_info.type = WebCursorInfo::TypeSouthResize;
      break;
    case kThemeResizeUpDownCursor:
      cursor_info.type = WebCursorInfo::TypeNorthSouthResize;
      break;
    case kThemePoofCursor:  // *shrug*
    default:
      cursor_info.type = WebCursorInfo::TypePointer;
      break;
  }

  InitFromCursorInfo(cursor_info);
}

void WebCursor::InitFromCursor(const Cursor* cursor) {
  // This conversion isn't perfect (in particular, the inversion effect of
  // data==1, mask==0 won't work). Not planning on fixing it.

  gfx::Size custom_size(16, 16);
  std::vector<char> raw_data;
  for (int row = 0; row < 16; ++row) {
    unsigned short data = cursor->data[row];
    unsigned short mask = cursor->mask[row];

    // The Core Endian flipper callback for 'CURS' doesn't flip Bits16 as if it
    // were a short (which it is), so we flip it here.
    data = ((data << 8) & 0xFF00) | ((data >> 8) & 0x00FF);
    mask = ((mask << 8) & 0xFF00) | ((mask >> 8) & 0x00FF);

    for (int bit = 0; bit < 16; ++bit) {
      if (data & 0x8000) {
        raw_data.push_back(0x00);
        raw_data.push_back(0x00);
        raw_data.push_back(0x00);
      } else {
        raw_data.push_back(0xFF);
        raw_data.push_back(0xFF);
        raw_data.push_back(0xFF);
      }
      if (mask & 0x8000)
        raw_data.push_back(0xFF);
      else
        raw_data.push_back(0x00);
      data <<= 1;
      mask <<= 1;
    }
  }

  base::mac::ScopedCFTypeRef<CGImageRef> cg_image(
      CreateCGImageFromCustomData(raw_data, custom_size));

  WebKit::WebCursorInfo cursor_info;
  cursor_info.type = WebCursorInfo::TypeCustom;
  cursor_info.hotSpot = WebKit::WebPoint(cursor->hotSpot.h, cursor->hotSpot.v);
  // TODO(avi): build the cursor image in Skia directly rather than going via
  // this roundabout path.
  cursor_info.customImage = gfx::CGImageToSkBitmap(cg_image.get());

  InitFromCursorInfo(cursor_info);
}

void WebCursor::InitFromNSCursor(NSCursor* cursor) {
  WebKit::WebCursorInfo cursor_info;

  if ([cursor isEqual:[NSCursor arrowCursor]]) {
    cursor_info.type = WebCursorInfo::TypePointer;
  } else if ([cursor isEqual:[NSCursor IBeamCursor]]) {
    cursor_info.type = WebCursorInfo::TypeIBeam;
  } else if ([cursor isEqual:[NSCursor crosshairCursor]]) {
    cursor_info.type = WebCursorInfo::TypeCross;
  } else if ([cursor isEqual:[NSCursor pointingHandCursor]]) {
    cursor_info.type = WebCursorInfo::TypeHand;
  } else if ([cursor isEqual:[NSCursor resizeLeftCursor]]) {
    cursor_info.type = WebCursorInfo::TypeWestResize;
  } else if ([cursor isEqual:[NSCursor resizeRightCursor]]) {
    cursor_info.type = WebCursorInfo::TypeEastResize;
  } else if ([cursor isEqual:[NSCursor resizeLeftRightCursor]]) {
    cursor_info.type = WebCursorInfo::TypeEastWestResize;
  } else if ([cursor isEqual:[NSCursor resizeUpCursor]]) {
    cursor_info.type = WebCursorInfo::TypeNorthResize;
  } else if ([cursor isEqual:[NSCursor resizeDownCursor]]) {
    cursor_info.type = WebCursorInfo::TypeSouthResize;
  } else if ([cursor isEqual:[NSCursor resizeUpDownCursor]]) {
    cursor_info.type = WebCursorInfo::TypeNorthSouthResize;
  } else if ([cursor isEqual:[NSCursor openHandCursor]]) {
    cursor_info.type = WebCursorInfo::TypeGrab;
  } else if ([cursor isEqual:[NSCursor closedHandCursor]]) {
    cursor_info.type = WebCursorInfo::TypeGrabbing;
  } else if ([cursor isEqual:[NSCursor operationNotAllowedCursor]]) {
    cursor_info.type = WebCursorInfo::TypeNotAllowed;
  } else if ([NSCursor respondsToSelector:@selector(dragCopyCursor)] &&
             [cursor isEqual:[NSCursor dragCopyCursor]]) {
    cursor_info.type = WebCursorInfo::TypeCopy;
  } else if ([NSCursor respondsToSelector:@selector(contextualMenuCursor)] &&
             [cursor isEqual:[NSCursor contextualMenuCursor]]) {
    cursor_info.type = WebCursorInfo::TypeContextMenu;
  } else if (
      [NSCursor respondsToSelector:@selector(IBeamCursorForVerticalLayout)] &&
      [cursor isEqual:[NSCursor IBeamCursorForVerticalLayout]]) {
    cursor_info.type = WebCursorInfo::TypeVerticalText;
  } else {
    // Also handles the [NSCursor disappearingItemCursor] case. Quick-and-dirty
    // image conversion; TODO(avi): do better.
    CGImageRef cg_image = nil;
    NSImage* image = [cursor image];
    for (id rep in [image representations]) {
      if ([rep isKindOfClass:[NSBitmapImageRep class]]) {
        cg_image = [rep CGImage];
        break;
      }
    }

    if (cg_image) {
      cursor_info.type = WebCursorInfo::TypeCustom;
      NSPoint hot_spot = [cursor hotSpot];
      cursor_info.hotSpot = WebKit::WebPoint(hot_spot.x, hot_spot.y);
      cursor_info.customImage = gfx::CGImageToSkBitmap(cg_image);
    } else {
      cursor_info.type = WebCursorInfo::TypePointer;
    }
  }

  InitFromCursorInfo(cursor_info);
}

void WebCursor::InitPlatformData() {
  return;
}

bool WebCursor::SerializePlatformData(Pickle* pickle) const {
  return true;
}

bool WebCursor::DeserializePlatformData(PickleIterator* iter) {
  return true;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  return;
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  return;
}
